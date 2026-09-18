#include "Systems/EnemyAiSystem.h"

#include "Actions/WaitAction.h"
#include "Combat/StatusEffectLibrary.h"
#include "Combat/StatusEffectLibraryFile.h"
#include "Combat/Technique.h"
#include "Combat/TechniqueLibrary.h"
#include "Components/ActorComponent.h"
#include "Components/AiComponent.h"
#include "Components/BlocksMovementComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/InnateWeaponComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RaceComponent.h"
#include "Components/RegisterComponents.h"
#include "Engine/Actions/ActionExecutor.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/JsonEntityLoader.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Render/VisualEffectSystem.h"
#include "Engine/World/Grid.h"
#include "Items/AffixLibrary.h"
#include "Systems/TurnCoordinator.h"
#include "Systems/TweenSystem.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/core/hashed_string.hpp>

#include <random>

// Reproduces a real freeze bug: a ChaseAndAttack enemy prefab authored without
// an "innate_weapon" component never gets an EquipmentComponent from
// GameplayLayer's on_enemy_spawned (App/Source/Layers/GameplayLayer.cpp's
// on_enemy_spawned only emplaces one `if (TryGetComponent<InnateWeaponComponent>(entity))`
// finds one). Without EquipmentComponent, EquipmentComponent::ContributeAttack
// never runs, so BeforeAttackEvent::has_weapon stays false. When
// EnemyAiSystem chases such an entity into melee range, MoveAction's
// bump-into-hostile fallback (MoveAction.cpp) hands off to a
// WeaponAttackAction, which now charges a normal attack cost (and logs a
// warning) for exactly this case instead of returning a zero-cost no-op --
// see WeaponAttackAction.cpp's `if (!before_attack.has_weapon)` branch. Before
// that safety net existed, a 0-cost result left the actor's TurnQueue energy
// unchanged, so the very next TurnCoordinator::Step loop iteration re-selected
// the same actor, which made the identical decision again -- Step() never
// returned, hanging the whole game on that actor's turn forever.
//
// enemies/gobooma.json shipped with exactly this gap (no innate_weapon) while
// its "wave"-mate enemies/booma.json had one; it's now fixed to carry
// innate_weapon (weapons.natural_weapon), same as booma.json. This file
// documents the mechanism against a synthetic fixture (so a *future* prefab
// that omits innate_weapon is still caught degrading safely rather than
// hanging), pins the real gobooma.json content, and end-to-end confirms
// TurnCoordinator::Step never hangs on it.

namespace {
psr::Registry MakeRegistryWithRealContent(psr::EntitySchemaModel& out_schema)
{
    psr::Registry registry;
    out_schema = psr::RegisterComponents(registry);

    // Mirrors GameplayLayer::SpawnPlayer's own setup order: affix/status
    // libraries must be installed before any EquipmentComponent/StatsComponent
    // entity exists, since EquipmentComponent's BeforeAttackEvent handler
    // reads Registry::GetAffixLibrary() unconditionally.
    static const psr::AffixLibrary affixes;
    static const psr::StatusEffectLibrary status_effects;
    registry.SetAffixLibrary(affixes);
    registry.SetStatusEffectLibrary(status_effects);

    psr::JsonEntityLoader loader{registry.GetMetaContext(), &out_schema};
    loader.Load("App/Assets/Data/Entities");
    registry.RegisterPrefabs(loader);
    return registry;
}

// Same "bake InnateWeaponComponent into a real equipped weapon instance"
// step GameplayLayer's on_enemy_spawned performs.
void ApplyOnSpawned(psr::Registry& registry, entt::entity entity)
{
    registry.GetOrEmplace<psr::ActorComponent>(entity);
    if (const auto* innate = registry.TryGetComponent<psr::InnateWeaponComponent>(entity))
    {
        const entt::entity weapon = registry.CreateEntity(innate->weapon_prefab_id);
        registry.Emplace<psr::EquipmentComponent>(entity, psr::EquipmentComponent{weapon});
    }
}
} // namespace

TEST_CASE("A chase_and_attack enemy missing innate_weapon bump-attacks a hostile for a nonzero cost",
          "[EnemyAiSystem][repro]")
{
    psr::EntitySchemaModel schema;
    psr::Registry registry = MakeRegistryWithRealContent(schema);

    psr::Grid grid(5, 5);
    psr::AffixLibrary affixes;
    psr::TechniqueLibrary techniques;
    std::mt19937 rng{0};
    psr::VisualEffectSystem visual_effects(registry, grid, [](entt::entity, std::uint8_t) {});
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, visual_effects, rng};

    // A synthetic stand-in for "authored chase_and_attack prefab, innate_weapon
    // omitted by mistake" -- exactly gobooma.json's pre-fix shape -- built by
    // hand rather than via prefab so this test still catches the bug class
    // even after every real prefab is fixed.
    const entt::entity attacker = registry.CreateEntity();
    registry.Emplace<psr::Position>(attacker, psr::Vec2{2, 2});
    registry.Emplace<psr::AiComponent>(attacker, psr::AiComponent{psr::AiBehavior::ChaseAndAttack, 8});
    registry.Emplace<psr::BlocksMovementComponent>(attacker);
    registry.Emplace<psr::HealthComponent>(attacker, psr::HealthComponent{20, 20});
    registry.Emplace<psr::RaceComponent>(attacker, psr::RaceComponent{entt::hashed_string::value("native")});
    ApplyOnSpawned(registry, attacker); // no InnateWeaponComponent present -- EquipmentComponent never gets emplaced
    grid.AddEntity(psr::Vec2{2, 2}, attacker);

    const entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::BlocksMovementComponent>(player); // real player.json has this -- without it, an
                                                             // attacker just walks onto the player's tile instead
                                                             // of bump-attacking, which would test the wrong path
    registry.Emplace<psr::Position>(player, psr::Vec2{3, 2}); // adjacent
    registry.Emplace<psr::HealthComponent>(player, psr::HealthComponent{100, 100});
    grid.AddEntity(psr::Vec2{3, 2}, player);

    psr::IAction* action = ai.Decide(psr::Entity(registry, attacker));
    REQUIRE(action != nullptr);
    const psr::ActionResult result = psr::ResolveAction(*action, psr::Entity(registry, attacker));

    // Pre-safety-net this was 0 -- the exact hazard that hangs TurnCoordinator::Step
    // (see this file's header comment). WeaponAttackAction's fallback now
    // charges a normal attack cost for a weapon-less NPC instead.
    CHECK(result.cost > 0);
}

TEST_CASE("Real gobooma prefab bump-attacks a hostile player for nonzero cost (post-fix)",
          "[EnemyAiSystem][repro]")
{
    psr::EntitySchemaModel schema;
    psr::Registry registry = MakeRegistryWithRealContent(schema);

    psr::Grid grid(5, 5);
    psr::AffixLibrary affixes;
    psr::TechniqueLibrary techniques;
    std::mt19937 rng{0};
    psr::VisualEffectSystem visual_effects(registry, grid, [](entt::entity, std::uint8_t) {});
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, visual_effects, rng};

    const std::uint32_t gobooma_id = entt::hashed_string::value("enemies.gobooma");
    REQUIRE(registry.HasPrefab(gobooma_id));

    const entt::entity gobooma = registry.CreateEntity(gobooma_id);
    registry.Emplace<psr::Position>(gobooma, psr::Vec2{2, 2});
    ApplyOnSpawned(registry, gobooma);
    grid.AddEntity(psr::Vec2{2, 2}, gobooma);

    const entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::BlocksMovementComponent>(player); // real player.json has this -- without it, an
                                                             // attacker just walks onto the player's tile instead
                                                             // of bump-attacking, which would test the wrong path
    registry.Emplace<psr::Position>(player, psr::Vec2{3, 2}); // adjacent
    registry.Emplace<psr::HealthComponent>(player, psr::HealthComponent{100, 100});
    grid.AddEntity(psr::Vec2{3, 2}, player);

    psr::IAction* action = ai.Decide(psr::Entity(registry, gobooma));
    REQUIRE(action != nullptr);
    const psr::ActionResult result = psr::ResolveAction(*action, psr::Entity(registry, gobooma));

    CHECK(result.cost > 0);
}

// Full TurnCoordinator-level confirmation: drives real Step() calls the same
// way GameplayLayer's game loop does, with a real gobooma chasing a
// stationary player. Pre-fix this TEST_CASE would never return (Step() spins
// forever on gobooma's turn); a bounded watchdog loop is the only way to
// assert "did not hang" -- a real regression here means the test process has
// to be force-killed rather than finishing.
TEST_CASE("Real gobooma chasing a player runs many turns without TurnCoordinator::Step ever hanging",
          "[EnemyAiSystem][repro]")
{
    psr::EntitySchemaModel schema;
    psr::Registry registry = MakeRegistryWithRealContent(schema);

    psr::Grid grid(10, 5);
    psr::AffixLibrary affixes;
    psr::TechniqueLibrary techniques;
    std::mt19937 rng{0};
    psr::VisualEffectSystem visual_effects(registry, grid, [](entt::entity, std::uint8_t) {});

    psr::TurnCoordinator coordinator(registry);
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, visual_effects, rng};
    coordinator.SetNpcDecision([&](psr::Entity actor) -> psr::IAction* { return ai.Decide(actor); });

    const entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::BlocksMovementComponent>(player); // real player.json has this -- without it, an
                                                             // attacker just walks onto the player's tile instead
                                                             // of bump-attacking, which would test the wrong path
    registry.Emplace<psr::ActorComponent>(player); // required precondition, see TurnCoordinator::Step's doc comment
    registry.Emplace<psr::Position>(player, psr::Vec2{7, 2});
    registry.Emplace<psr::HealthComponent>(player, psr::HealthComponent{500, 500});
    grid.AddEntity(psr::Vec2{7, 2}, player);

    const std::uint32_t gobooma_id = entt::hashed_string::value("enemies.gobooma");
    REQUIRE(registry.HasPrefab(gobooma_id));
    const entt::entity gobooma = registry.CreateEntity(gobooma_id);
    registry.Emplace<psr::Position>(gobooma, psr::Vec2{1, 2});
    ApplyOnSpawned(registry, gobooma);
    coordinator.Subscribe(psr::Entity(registry, gobooma));
    grid.AddEntity(psr::Vec2{1, 2}, gobooma);

    psr::WaitAction wait_action;
    int player_turns = 0;
    constexpr int kWatchdogStepLimit = 5000;
    for (int i = 0; i < kWatchdogStepLimit && player_turns < 20; ++i)
    {
        coordinator.SetPendingAction(&wait_action);
        const psr::TurnStep step = coordinator.Step(1.0f / 60.0f);

        if (step == psr::TurnStep::AnimationsPending)
            psr::UpdateTweens(registry, 10.0f); // instantly flush so the loop never stalls on a real animation
        else if (step == psr::TurnStep::Resolved)
            ++player_turns;
        else if (step == psr::TurnStep::PlayerDefeated)
            break;
    }

    CHECK(player_turns > 0);
}
