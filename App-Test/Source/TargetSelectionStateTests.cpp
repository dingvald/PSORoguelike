#include "States/TargetSelectionState.h"

#include "Actions/WaitAction.h"
#include "Combat/StatusEffectLibrary.h"
#include "Components/EnergyComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RenderableComponent.h"
#include "Components/SelectedTargetComponent.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/IEntityLoader.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/World/Grid.h"
#include "Systems/TurnCoordinator.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/core/hashed_string.hpp>

#include <SDL3/SDL_keycode.h>

namespace {

const std::uint32_t kCursorPrefabId = entt::hashed_string::value("ui.target_select_cursor");

// A minimal stand-in for the real target_select_cursor.json prefab -- just
// enough (a RenderableComponent) for TargetSelectionState::OnEnter/
// UpdateCursorVisual to have something to spawn and recolor.
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
    }
};

struct Fixture
{
    psr::Registry registry;
    psr::Grid grid{7, 7};
    psr::StatusEffectLibrary status_effects;
    psr::TurnCoordinator turn_coordinator{registry};
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

    psr::GameplayContext Context() { return psr::GameplayContext{registry, grid, turn_coordinator, actor}; }

    bool Send(psr::TargetSelectionState& state, psr::GameplayContext& context, int key_code)
    {
        psr::KeyPressedEvent key_event(key_code, /*repeat=*/false);
        return state.HandleEvent(key_event, context);
    }
};

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
    fixture.registry.Emplace<psr::EnergyComponent>(fixture.actor);

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
    CHECK(fixture.registry.GetComponent<psr::EnergyComponent>(fixture.actor).energy == 0);
}
