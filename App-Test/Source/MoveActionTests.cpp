#include "Actions/AttackAction.h"
#include "Actions/MoveAction.h"

#include "Combat/StatusEffectApplication.h"
#include "CombatRegistrySetup.h"
#include "Components/ActorComponent.h"
#include "Components/BlocksMovementComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/StatsComponent.h"
#include "Components/StatusEffectComponent.h"
#include "Components/TweenComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/Actions/ActionExecutor.h"
#include "Engine/Actions/MoveEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>

namespace {
psr::AffixLibrary g_no_affixes;
psr::StatusEffectLibrary g_no_status_effects;
} // namespace

TEST_CASE("MoveAction moves an actor to an open tile", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{1, 0}, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::MoveAction::kMoveCost);
    REQUIRE(actor.Get<psr::Position>().tile == psr::Vec2{2, 1});
    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}).empty());
    REQUIRE(grid.GetEntities(psr::Vec2{2, 1}) == std::vector<entt::entity>{handle});
}

TEST_CASE("MoveAction's cost is scaled by the actor's ActorComponent::movement_speed", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    actor.Emplace<psr::ActorComponent>(psr::ActorComponent{0, 200, 100});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{1, 0}, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::MoveAction::kMoveCost / 2);
}

TEST_CASE("MoveAction emplaces a TweenComponent starting at the pre-move offset", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{1, 0}, rng);
    action.Perform(actor);

    const psr::TweenComponent& tween_component = actor.Get<psr::TweenComponent>();
    REQUIRE(tween_component.queue.size() == 1);
    const psr::Tween& tween = tween_component.queue.front();
    REQUIRE(tween.start_offset == psr::Vec2f{-1.0f, 0.0f});
    REQUIRE(tween.end_offset == psr::Vec2f{});
    REQUIRE(tween.duration == psr::MoveAction::kMoveTweenDuration);
    REQUIRE(tween.elapsed == 0.0f);
}

TEST_CASE("MoveAction targeting an out-of-bounds tile is a free no-op", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{0, 0});
    grid.AddEntity(psr::Vec2{0, 0}, handle);

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{-1, 0}, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE(actor.Get<psr::Position>().tile == psr::Vec2{0, 0});
    REQUIRE(grid.GetEntities(psr::Vec2{0, 0}) == std::vector<entt::entity>{handle});
    REQUIRE_FALSE(actor.Has<psr::TweenComponent>());
}

TEST_CASE("MoveAction blocked by a non-attackable BlocksMovementComponent occupant is a free no-op", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    entt::entity blocker = registry.CreateEntity();
    registry.Emplace<psr::BlocksMovementComponent>(blocker);
    grid.AddEntity(psr::Vec2{2, 1}, blocker);

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{1, 0}, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE_FALSE(result.fallback);
    REQUIRE(actor.Get<psr::Position>().tile == psr::Vec2{1, 1});
    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{handle});
}

TEST_CASE("MoveAction onto a tile occupied by a non-blocking entity still succeeds", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    entt::entity item = registry.CreateEntity();
    grid.AddEntity(psr::Vec2{2, 1}, item);

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{1, 0}, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::MoveAction::kMoveCost);
    REQUIRE(grid.GetEntities(psr::Vec2{2, 1}) == std::vector<entt::entity>{item, handle});
}

TEST_CASE("MoveAction bumping into a hostile attackable occupant falls back to an AttackAction", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    psr::SetUpCombatRegistry(registry, grid, g_no_affixes, g_no_status_effects);
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);
    actor.Emplace<psr::PlayerControlledComponent>();

    // Bump-to-attack needs a weapon equipped (AttackAction is a free no-op
    // otherwise), consistent with attacking via any other route.
    entt::entity weapon = registry.CreateEntity();
    registry.Emplace<psr::WeaponComponent>(weapon);
    registry.Emplace<psr::StatsComponent>(weapon);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    psr::StatsComponent& actor_stats = actor.GetOrEmplace<psr::StatsComponent>();
    actor_stats.atp = 80;
    actor_stats.ata = 200;

    entt::entity enemy_handle = registry.CreateEntity();
    psr::Entity enemy(registry, enemy_handle);
    registry.Emplace<psr::BlocksMovementComponent>(enemy_handle);
    psr::HealthComponent enemy_health;
    enemy_health.current_hp = 10;
    enemy_health.max_hp = 10;
    enemy.Emplace<psr::HealthComponent>(enemy_health);
    grid.AddEntity(psr::Vec2{2, 1}, enemy_handle);

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{1, 0}, rng);
    psr::ActionResult result = psr::ResolveAction(action, actor); // runs the AttackAction fallback in the same turn

    // The bump itself is free (cost 0); only the resolved AttackAction's own
    // cost is ever applied -- actor never actually steps onto the enemy's
    // tile. The fallback AttackAction found a target, so it queues its own
    // lunge-and-return Tween pair on the actor -- MoveAction itself never
    // emplaces one in this branch (see MoveAction.h's own doc comment).
    REQUIRE(result.cost == psr::AttackAction::kAttackCost);
    REQUIRE(actor.Get<psr::Position>().tile == psr::Vec2{1, 1});
    REQUIRE(actor.Get<psr::TweenComponent>().queue.size() == 2);
}

TEST_CASE("MoveAction is a free no-op when a BeforeMoveEvent handler cancels it", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    struct RootProbe
    {
    };
    actor.Get<psr::EventHandlerComponent>().Subscribe<psr::BeforeMoveEvent, RootProbe>(
        [](psr::Entity, psr::BeforeMoveEvent& event) { event.cancelled = true; });

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{1, 0}, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE(actor.Get<psr::Position>().tile == psr::Vec2{1, 1});
}

TEST_CASE("MoveAction dispatches AfterMoveEvent with the from/to tiles on a successful move", "[MoveAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    struct MoveProbe
    {
    };
    std::optional<psr::AfterMoveEvent> received;
    actor.Get<psr::EventHandlerComponent>().Subscribe<psr::AfterMoveEvent, MoveProbe>(
        [&](psr::Entity, psr::AfterMoveEvent& event) { received = event; });

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{1, 0}, rng);
    action.Perform(actor);

    REQUIRE(received.has_value());
    REQUIRE(received->from == psr::Vec2{1, 1});
    REQUIRE(received->to == psr::Vec2{2, 1});
}

TEST_CASE("MoveAction moves to the BeforeMoveEvent handler's redirected offset, not the originally-requested one",
          "[MoveAction]")
{
    // Deterministic regression guard for the "read offset back after
    // dispatch" contract StatusEffectComponent's Confuse handling relies on
    // -- independent of that handler's own RNG, a redirect to any offset
    // must actually be consumed by MoveAction rather than the original
    // m_offset silently winning.
    psr::Registry registry;
    psr::Grid grid{3, 3};
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    grid.AddEntity(psr::Vec2{1, 1}, handle);

    struct RedirectProbe
    {
    };
    actor.Get<psr::EventHandlerComponent>().Subscribe<psr::BeforeMoveEvent, RedirectProbe>(
        [](psr::Entity, psr::BeforeMoveEvent& event) { event.offset = psr::Vec2{0, 1}; });

    std::mt19937 rng{1};
    psr::MoveAction action(grid, g_no_affixes, psr::Vec2{1, 0}, rng); // requested: right
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::MoveAction::kMoveCost);
    REQUIRE(actor.Get<psr::Position>().tile == psr::Vec2{1, 2}); // redirected: down, not right
}

TEST_CASE("MoveAction redirects to a random cardinal direction while the actor is Confused",
          "[MoveAction][StatusEffect]")
{
    psr::StatusEffect confuse;
    confuse.id = 1;
    confuse.type = psr::StatusEffectType::Confuse;
    confuse.duration = 1000; // never expires across the trials below
    psr::StatusEffectLibrary status_effects{{confuse}};

    const psr::Vec2 origin{2, 2};
    bool saw_a_redirect = false;

    // Repeated trials rather than a single roll -- StatusEffectComponent's
    // Confuse handler owns its own self-seeded RNG (see its doc comment),
    // so this can't be seeded from the test. With a 1-in-4 chance per trial
    // of coincidentally re-picking the requested direction, the odds of all
    // 100 trials doing so are astronomically small, so this is deterministic
    // in practice while still exercising the real (non-seedable) code path.
    for (int trial = 0; trial < 100; ++trial)
    {
        psr::Registry registry;
        psr::Grid grid{5, 5};
        registry.SetStatusEffectLibrary(status_effects);
        registry.BindComponentEvents<psr::StatusEffectComponent>();
        entt::entity handle = registry.CreateEntity();
        psr::Entity actor(registry, handle);
        actor.Emplace<psr::Position>(origin);
        grid.AddEntity(origin, handle);
        psr::ApplyStatusEffect(actor, status_effects, confuse.id);

        std::mt19937 rng{1};
        psr::MoveAction action(grid, g_no_affixes, psr::Vec2{1, 0}, rng); // requested: right
        psr::ActionResult result = action.Perform(actor);

        REQUIRE(result.cost == psr::MoveAction::kMoveCost);
        const psr::Vec2 landed = actor.Get<psr::Position>().tile;
        const psr::Vec2 delta = landed - origin;
        // Always one of the 4 cardinal neighbours -- never the origin tile,
        // never a diagonal.
        REQUIRE(((delta == psr::Vec2{0, -1}) || (delta == psr::Vec2{0, 1}) || (delta == psr::Vec2{-1, 0}) ||
                 (delta == psr::Vec2{1, 0})));
        if (delta != psr::Vec2{1, 0})
            saw_a_redirect = true;
    }

    REQUIRE(saw_a_redirect);
}
