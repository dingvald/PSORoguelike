#include "States/TargetSelectionState.h"

#include "Actions/WaitAction.h"
#include "Combat/StatusEffectLibrary.h"
#include "Components/ActorComponent.h"
#include "Components/BlocksMovementComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RenderableComponent.h"
#include "Components/SelectedTargetComponent.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/IEntityLoader.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/World/Grid.h"
#include "Systems/TurnCoordinator.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/core/hashed_string.hpp>

#include <SDL3/SDL_keycode.h>

namespace {

const std::uint32_t kCursorPrefabId = entt::hashed_string::value("ui.target_select_cursor");
const std::uint32_t kTravelPreviewPrefabId = entt::hashed_string::value("ui.target_travel_preview");
const std::uint32_t kAreaPreviewPrefabId = entt::hashed_string::value("ui.target_area_preview");

// A minimal stand-in for the real target_select_cursor.json/
// target_travel_preview.json/target_area_preview.json prefabs -- just enough
// (a RenderableComponent) for TargetSelectionState::OnEnter/UpdateCursorVisual/
// UpdatePreview to have something to spawn and recolor.
class CursorEntityLoader : public psr::IEntityLoader
{
public:
    bool Load(std::filesystem::path /*path*/) override { return true; }

    void Populate(entt::registry& prefab_registry,
                  std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
    {
        entt::entity prefab = prefab_registry.create();
        psr::RenderableComponent renderable;
        renderable.color_1 = psr::Color{200, 100, 50, 255};
        renderable.color_2 = renderable.color_1;
        prefab_registry.emplace<psr::RenderableComponent>(prefab, renderable);
        out_prefab_ids.emplace(kCursorPrefabId, prefab);

        for (std::uint32_t preview_prefab_id : {kTravelPreviewPrefabId, kAreaPreviewPrefabId})
        {
            entt::entity preview_prefab = prefab_registry.create();
            prefab_registry.emplace<psr::RenderableComponent>(preview_prefab, renderable);
            out_prefab_ids.emplace(preview_prefab_id, preview_prefab);
        }
    }
};

struct Fixture
{
    psr::Registry registry;
    psr::Grid grid{7, 7};
    psr::StatusEffectLibrary status_effects;
    psr::TurnCoordinator turn_coordinator{registry};
    psr::MessageBus message_bus;
    entt::entity actor = entt::null;
    psr::WaitAction dummy_action;

    Fixture()
    {
        psr::ComponentSchemaRegistrar reg{registry.GetMetaContext()};
        psr::RenderableComponent::Register(reg);

        // TurnCoordinator's Freeze check calls Registry::GetStatusEffectLibrary()
        // unconditionally on every actor's turn (see TurnCoordinator.cpp) --
        // needed here since the one test that calls turn_coordinator.Step()
        // would otherwise trip that call's "was SetStatusEffectLibrary() ever
        // called?" assert, per CombatRegistrySetup.h's identical rationale.
        registry.SetStatusEffectLibrary(status_effects);

        CursorEntityLoader loader;
        registry.RegisterPrefabs(loader);

        actor = registry.CreateEntity();
        registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{3, 3}});
        grid.AddEntity(psr::Vec2{3, 3}, actor);
    }

    psr::GameplayContext Context()
    {
        return psr::GameplayContext{registry, grid, turn_coordinator, actor, message_bus};
    }

    bool Send(psr::TargetSelectionState& state, psr::GameplayContext& context, int key_code)
    {
        psr::KeyPressedEvent key_event(key_code, /*repeat=*/false);
        return state.HandleEvent(key_event, context);
    }
};

// Counts the preview entities of the given prefab currently occupying tile,
// identified via Registry::CreateEntity(prefab_id)'s own PrefabIdComponent
// stamp -- lets the tests tell travel-preview and area-preview entities apart
// even though CursorEntityLoader gives every prefab an identical
// RenderableComponent.
int CountEntitiesWithPrefab(psr::Registry& registry, psr::Grid& grid, psr::Vec2 tile, std::uint32_t prefab_id)
{
    int count = 0;
    for (entt::entity entity : grid.GetEntities(tile))
    {
        const psr::PrefabIdComponent* prefab = registry.TryGetComponent<psr::PrefabIdComponent>(entity);
        if (prefab && prefab->value == prefab_id)
            ++count;
    }
    return count;
}

} // namespace

TEST_CASE("TargetSelectionState SelfTarget keeps the cursor fixed at origin and always confirms",
          "[TargetSelectionState]")
{
    Fixture fixture;
    psr::TargetSelectionState state;
    psr::TargetRequest request;
    request.action = &fixture.dummy_action;
    request.mode = psr::TargetingMode::SelfTarget;
    state.Begin(request, fixture.actor);

    psr::GameplayContext context = fixture.Context();
    state.OnEnter(context);

    fixture.Send(state, context, SDLK_RIGHT); // SelfTarget ignores movement
    fixture.Send(state, context, SDLK_SPACE);

    psr::StateTransition transition = state.Update(context, 0.016f);
    REQUIRE(transition.kind == psr::StateTransitionKind::Pop);
    REQUIRE(fixture.registry.HasComponent<psr::SelectedTargetComponent>(fixture.actor));
    CHECK(fixture.registry.GetComponent<psr::SelectedTargetComponent>(fixture.actor).tile == psr::Vec2{3, 3});

    state.OnExit(context);
}

TEST_CASE("TargetSelectionState Directional jumps the cursor straight to the pressed cardinal neighbour",
          "[TargetSelectionState]")
{
    Fixture fixture;
    psr::TargetSelectionState state;
    psr::TargetRequest request;
    request.action = &fixture.dummy_action;
    request.mode = psr::TargetingMode::Directional;
    state.Begin(request, fixture.actor);

    psr::GameplayContext context = fixture.Context();
    state.OnEnter(context); // default facing: up, i.e. cursor at {3,2}

    fixture.Send(state, context, SDLK_RIGHT); // jump straight to {4,3}, not an incremental step from {3,2}
    fixture.Send(state, context, SDLK_SPACE); // Directional is always reachable

    psr::StateTransition transition = state.Update(context, 0.016f);
    REQUIRE(transition.kind == psr::StateTransitionKind::Pop);
    CHECK(fixture.registry.GetComponent<psr::SelectedTargetComponent>(fixture.actor).tile == psr::Vec2{4, 3});

    state.OnExit(context);
}

TEST_CASE("TargetSelectionState TargetSquare only confirms within Chebyshev range", "[TargetSelectionState]")
{
    Fixture fixture;
    psr::TargetSelectionState state;
    psr::TargetRequest request;
    request.action = &fixture.dummy_action;
    request.mode = psr::TargetingMode::TargetSquare;
    request.range = 2;
    state.Begin(request, fixture.actor);

    psr::GameplayContext context = fixture.Context();
    state.OnEnter(context); // cursor starts at origin {3,3}

    // Three tiles right -> {6,3}, Chebyshev distance 3 > range 2: out of range.
    fixture.Send(state, context, SDLK_RIGHT);
    fixture.Send(state, context, SDLK_RIGHT);
    fixture.Send(state, context, SDLK_RIGHT);
    fixture.Send(state, context, SDLK_SPACE);

    psr::StateTransition out_of_range = state.Update(context, 0.016f);
    CHECK(out_of_range.kind == psr::StateTransitionKind::None); // confirm was rejected, no Pop yet
    CHECK_FALSE(fixture.registry.HasComponent<psr::SelectedTargetComponent>(fixture.actor));

    // Step back one tile -> {5,3}, Chebyshev distance 2 == range: in range.
    fixture.Send(state, context, SDLK_LEFT);
    fixture.Send(state, context, SDLK_SPACE);

    psr::StateTransition in_range = state.Update(context, 0.016f);
    REQUIRE(in_range.kind == psr::StateTransitionKind::Pop);
    CHECK(fixture.registry.GetComponent<psr::SelectedTargetComponent>(fixture.actor).tile == psr::Vec2{5, 3});

    state.OnExit(context);
}

TEST_CASE("TargetSelectionState cancel pops without writing SelectedTargetComponent", "[TargetSelectionState]")
{
    Fixture fixture;
    psr::TargetSelectionState state;
    psr::TargetRequest request;
    request.action = &fixture.dummy_action;
    request.mode = psr::TargetingMode::Directional;
    state.Begin(request, fixture.actor);

    psr::GameplayContext context = fixture.Context();
    state.OnEnter(context);

    fixture.Send(state, context, SDLK_ESCAPE);
    psr::StateTransition transition = state.Update(context, 0.016f);

    REQUIRE(transition.kind == psr::StateTransitionKind::Pop);
    CHECK_FALSE(fixture.registry.HasComponent<psr::SelectedTargetComponent>(fixture.actor));

    state.OnExit(context);
}

TEST_CASE("TargetSelectionState clamps cursor movement to grid bounds", "[TargetSelectionState]")
{
    Fixture fixture;
    // Move the actor to the grid's top-left corner so "up"/"left" would fall
    // off the grid.
    fixture.grid.RemoveEntity(psr::Vec2{3, 3}, fixture.actor);
    fixture.registry.GetComponent<psr::Position>(fixture.actor).tile = psr::Vec2{0, 0};
    fixture.grid.AddEntity(psr::Vec2{0, 0}, fixture.actor);

    psr::TargetSelectionState state;
    psr::TargetRequest request;
    request.action = &fixture.dummy_action;
    request.mode = psr::TargetingMode::TargetSquare;
    request.range = 5;
    state.Begin(request, fixture.actor);

    psr::GameplayContext context = fixture.Context();
    state.OnEnter(context); // cursor starts at {0,0}

    fixture.Send(state, context, SDLK_UP);   // off-grid -- no-op
    fixture.Send(state, context, SDLK_LEFT); // off-grid -- no-op
    fixture.Send(state, context, SDLK_SPACE);

    psr::StateTransition transition = state.Update(context, 0.016f);
    REQUIRE(transition.kind == psr::StateTransitionKind::Pop);
    CHECK(fixture.registry.GetComponent<psr::SelectedTargetComponent>(fixture.actor).tile == psr::Vec2{0, 0});

    state.OnExit(context);
}

TEST_CASE("TargetSelectionState confirm hands the wrapped action to TurnCoordinator via SetPendingAction",
          "[TargetSelectionState]")
{
    Fixture fixture;
    fixture.registry.Emplace<psr::PlayerControlledComponent>(fixture.actor);
    fixture.registry.Emplace<psr::ActorComponent>(fixture.actor);

    psr::TargetSelectionState state;
    psr::TargetRequest request;
    request.action = &fixture.dummy_action;
    request.mode = psr::TargetingMode::SelfTarget;
    state.Begin(request, fixture.actor);

    psr::GameplayContext context = fixture.Context();
    state.OnEnter(context);
    fixture.Send(state, context, SDLK_SPACE);
    state.Update(context, 0.016f);
    state.OnExit(context);

    psr::TurnStep step = fixture.turn_coordinator.Step(0.016f);

    REQUIRE(step == psr::TurnStep::Resolved);
    // TurnQueue's NextActor() fast-forwards a freshly-enqueued actor's energy
    // up to the action threshold before Step() resolves anything -- see
    // TurnCoordinatorTests.cpp's own "resolves a bound key" case, which
    // checks this same post-WaitAction value.
    CHECK(fixture.registry.GetComponent<psr::ActorComponent>(fixture.actor).ap == 0);
}

TEST_CASE("TargetSelectionState projectile preview mirrors BuildProjectilePath and clears at zero offset",
          "[TargetSelectionState]")
{
    Fixture fixture;
    // Wall three tiles right of the actor {3,3} -- BuildProjectilePath (and
    // this preview) must stop just short of it, same as the real cast would.
    const entt::entity wall = fixture.registry.CreateEntity();
    fixture.registry.Emplace<psr::BlocksMovementComponent>(wall);
    fixture.grid.AddEntity(psr::Vec2{6, 3}, wall);

    psr::TargetSelectionState state;
    psr::TargetRequest request;
    request.action = &fixture.dummy_action;
    request.mode = psr::TargetingMode::TargetSquare;
    request.range = 4;
    request.is_projectile = true;
    request.projectile_pierces = false;
    state.Begin(request, fixture.actor);

    psr::GameplayContext context = fixture.Context();
    state.OnEnter(context); // cursor starts at origin {3,3}: zero offset, no preview yet

    CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, psr::Vec2{4, 3}, kTravelPreviewPrefabId) == 0);
    CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, psr::Vec2{5, 3}, kAreaPreviewPrefabId) == 0);

    // One step right -> cursor {4,3}, direction {1,0}: path is {4,3},{5,3},
    // stopping just short of the wall at {6,3}. Non-piercing, so the area
    // (impact) preview is only the last path tile, {5,3}.
    fixture.Send(state, context, SDLK_RIGHT);

    CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, psr::Vec2{4, 3}, kTravelPreviewPrefabId) == 1);
    CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, psr::Vec2{4, 3}, kAreaPreviewPrefabId) == 0);
    CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, psr::Vec2{5, 3}, kTravelPreviewPrefabId) == 1);
    CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, psr::Vec2{5, 3}, kAreaPreviewPrefabId) == 1);
    CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, psr::Vec2{6, 3}, kTravelPreviewPrefabId) == 0);

    // A second step right along the same direction resolves the identical
    // path -- proves the clear-and-respawn cycle doesn't accumulate
    // duplicates at the unchanged tiles.
    fixture.Send(state, context, SDLK_RIGHT);

    CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, psr::Vec2{4, 3}, kTravelPreviewPrefabId) == 1);
    CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, psr::Vec2{5, 3}, kTravelPreviewPrefabId) == 1);
    CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, psr::Vec2{5, 3}, kAreaPreviewPrefabId) == 1);

    // Back to the origin -- zero offset clears every preview entity.
    fixture.Send(state, context, SDLK_LEFT);
    fixture.Send(state, context, SDLK_LEFT);

    CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, psr::Vec2{4, 3}, kTravelPreviewPrefabId) == 0);
    CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, psr::Vec2{5, 3}, kTravelPreviewPrefabId) == 0);
    CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, psr::Vec2{5, 3}, kAreaPreviewPrefabId) == 0);

    state.OnExit(context);
}

TEST_CASE("TargetSelectionState projectile preview includes every path tile in the area preview when piercing",
          "[TargetSelectionState]")
{
    Fixture fixture;

    psr::TargetSelectionState state;
    psr::TargetRequest request;
    request.action = &fixture.dummy_action;
    request.mode = psr::TargetingMode::TargetSquare;
    request.range = 3;
    request.is_projectile = true;
    request.projectile_pierces = true;
    state.Begin(request, fixture.actor);

    psr::GameplayContext context = fixture.Context();
    state.OnEnter(context); // cursor at origin {3,3}

    fixture.Send(state, context, SDLK_RIGHT); // cursor {4,3}, direction {1,0}, path {4,3},{5,3},{6,3}

    for (psr::Vec2 tile : {psr::Vec2{4, 3}, psr::Vec2{5, 3}, psr::Vec2{6, 3}})
    {
        CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, tile, kTravelPreviewPrefabId) == 1);
        CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, tile, kAreaPreviewPrefabId) == 1);
    }

    state.OnExit(context);
}

TEST_CASE("TargetSelectionState renders no projectile preview for a non-projectile request", "[TargetSelectionState]")
{
    Fixture fixture;

    psr::TargetSelectionState state;
    psr::TargetRequest request;
    request.action = &fixture.dummy_action;
    request.mode = psr::TargetingMode::TargetSquare;
    request.range = 4;
    // is_projectile defaults to false, matching a melee Photon Art/instant cast.
    state.Begin(request, fixture.actor);

    psr::GameplayContext context = fixture.Context();
    state.OnEnter(context);
    fixture.Send(state, context, SDLK_RIGHT);
    fixture.Send(state, context, SDLK_RIGHT);

    for (psr::Vec2 tile : {psr::Vec2{3, 3}, psr::Vec2{4, 3}, psr::Vec2{5, 3}, psr::Vec2{6, 3}})
    {
        CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, tile, kTravelPreviewPrefabId) == 0);
        CHECK(CountEntitiesWithPrefab(fixture.registry, fixture.grid, tile, kAreaPreviewPrefabId) == 0);
    }

    state.OnExit(context);
}
