#include "Actions/WeaponAttackAction.h"

#include "Combat/AttackEvent.h"
#include "Combat/StatusEffectApplication.h"
#include "CombatRegistrySetup.h"
#include "Components/ActorComponent.h"
#include "Components/BlocksMovementComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/KnockbackMultiplierComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/ProjectileComponent.h"
#include "Components/RaceComponent.h"
#include "Components/SelectedTargetComponent.h"
#include "Components/StatsComponent.h"
#include "Components/StatusEffectComponent.h"
#include "Components/TweenComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/Actions/ActionExecutor.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Systems/TweenSystem.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>

namespace {

// WeaponAttackAction no longer applies damage inline -- it queues a lunge-
// and-return Tween pair with the hit-resolution loop captured as the lunge's
// on_completion (see WeaponAttackAction.cpp's own doc comment). A single huge
// delta_time cascades UpdateTweens through both queued Tweens (and fires the
// damage callback) in a bounded loop, standing in for AnimationState's real
// per-frame drive.
void DrainAttackTween(psr::Registry& registry)
{
    while (registry.Any<psr::TweenComponent>())
        psr::UpdateTweens(registry, 999.0f);
}

// Tag type identifying this test file's subscriptions to EventHandlerComponent
// -- never instantiated, only used as Subscribe/Unsubscribe's TOwner key.
struct DamageEventProbe
{
};

// A weapon with generous ATP/ATA so hits are guaranteed (ComputeHitChance's
// Accuracy = ATA - EVP*0.2 clamps to 1.0 once ATA alone exceeds 100) and
// damage is well above 1, keeping the fixed-seed tests below non-flaky in
// practice without hand-verifying individual RNG draws.
entt::entity MakeWeapon(psr::Registry& registry, psr::WeaponRangeShape shape = psr::WeaponRangeShape::SingleTarget,
                        int range = 1, int hits_per_turn = 1)
{
    entt::entity weapon = registry.CreateEntity();
    psr::WeaponComponent component;
    component.range_shape = shape;
    component.range = range;
    component.hits_per_turn = hits_per_turn;
    registry.Emplace<psr::WeaponComponent>(weapon, component);
    registry.Emplace<psr::StatsComponent>(weapon); // no weapon stat bonus needed for these tests
    return weapon;
}

psr::Entity MakeActor(psr::Registry& registry, psr::Grid& grid, psr::Vec2 tile, int atp, int ata, bool player)
{
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(tile);
    grid.AddEntity(tile, handle);
    psr::StatsComponent stats;
    stats.atp = atp;
    stats.ata = ata;
    actor.Emplace<psr::StatsComponent>(stats);
    if (player)
        actor.Emplace<psr::PlayerControlledComponent>();
    return actor;
}

psr::Entity MakeDefender(psr::Registry& registry, psr::Grid& grid, psr::Vec2 tile, int dfp, int evp, int hp,
                         bool player)
{
    psr::Entity defender = MakeActor(registry, grid, tile, /*atp=*/0, /*ata=*/0, player);
    psr::StatsComponent& stats = defender.Get<psr::StatsComponent>();
    stats.dfp = dfp;
    stats.evp = evp;
    psr::HealthComponent health;
    health.current_hp = hp;
    health.max_hp = hp;
    defender.Emplace<psr::HealthComponent>(health);
    return defender;
}

} // namespace

TEST_CASE("WeaponAttackAction with no weapon equipped is a free no-op", "[WeaponAttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/50, /*player=*/true);

    psr::WeaponAttackAction action(grid, affixes, rng, psr::Vec2{1, 0});
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("WeaponAttackAction against an empty tile is a free no-op", "[WeaponAttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/50, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::WeaponAttackAction action(grid, affixes, rng, psr::Vec2{1, 0});
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("WeaponAttackAction does not damage a non-hostile occupant", "[WeaponAttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/50, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    // Same side (both player-controlled) -- not hostile per IsHostile.
    psr::Entity ally = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/20, /*player=*/true);

    psr::WeaponAttackAction action(grid, affixes, rng, psr::Vec2{1, 0});
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE(ally.Get<psr::HealthComponent>().current_hp == 20);
}

TEST_CASE("WeaponAttackAction eventually destroys a hostile SingleTarget occupant", "[WeaponAttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10, /*player=*/false);
    const entt::entity enemy_handle = enemy.Handle();

    psr::WeaponAttackAction action(grid, affixes, rng, psr::Vec2{1, 0});

    bool destroyed = false;
    for (int attempt = 0; attempt < 50 && !destroyed; ++attempt)
    {
        psr::ActionResult result = action.Perform(actor);
        REQUIRE(result.cost == psr::WeaponAttackAction::kWeaponAttackCost); // a hostile target was always found
        DrainAttackTween(registry);
        if (!registry.IsValid(enemy_handle))
            destroyed = true;
    }

    REQUIRE(destroyed);
}

TEST_CASE("WeaponAttackAction with hits_per_turn > 1 rolls multiple hits per Perform", "[WeaponAttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{7};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry, psr::WeaponRangeShape::SingleTarget, /*range=*/1, /*hits_per_turn=*/5);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    // High HP so a single hit_per_turn iteration can't kill it outright --
    // isolates "multiple hits landed in one Perform" from the death path.
    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10000, /*player=*/false);

    psr::WeaponAttackAction action(grid, affixes, rng, psr::Vec2{1, 0});
    psr::ActionResult result = action.Perform(actor);
    DrainAttackTween(registry);

    REQUIRE(result.cost == psr::WeaponAttackAction::kWeaponAttackCost);
    // ATP 80 vs DFP 0 -> floor((80-0)/5*0.9*variance) caps a single hit at 15
    // (variance <= 1.1); five guaranteed hits clearing more than that proves
    // multiple hits landed in one Perform().
    const int damage_dealt = 10000 - enemy.Get<psr::HealthComponent>().current_hp;
    REQUIRE(damage_dealt > 15);
}

TEST_CASE("WeaponAttackAction's cost is scaled by the actor's ActorComponent::act_speed", "[WeaponAttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    actor.Emplace<psr::ActorComponent>(psr::ActorComponent{0, 100, 50});

    MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10000, /*player=*/false);

    psr::WeaponAttackAction action(grid, affixes, rng, psr::Vec2{1, 0});
    psr::ActionResult result = action.Perform(actor);
    DrainAttackTween(registry);

    REQUIRE(result.cost == psr::WeaponAttackAction::kWeaponAttackCost * 2);
}

TEST_CASE("WeaponAttackAction applies a matching race bonus", "[WeaponAttackAction]")
{
    psr::AffixLibrary affixes;
    const std::uint32_t matching_race = 111;
    const std::uint32_t other_race = 222;

    auto RunAttack = [&](std::uint32_t defender_race) -> int
    {
        psr::Registry registry;
        psr::Grid grid{5, 5};
        psr::StatusEffectLibrary status_effects;
        psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
        std::mt19937 rng{42};

        psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
        entt::entity weapon = MakeWeapon(registry);
        psr::WeaponComponent& weapon_component = registry.GetComponent<psr::WeaponComponent>(weapon);
        weapon_component.race_bonuses.push_back({matching_race, /*bonus_percent=*/50});
        actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

        psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10000, /*player=*/false);
        enemy.Emplace<psr::RaceComponent>(psr::RaceComponent{defender_race});

        psr::WeaponAttackAction action(grid, affixes, rng, psr::Vec2{1, 0});
        action.Perform(actor);
        DrainAttackTween(registry);
        return 10000 - enemy.Get<psr::HealthComponent>().current_hp;
    };

    const int damage_with_bonus = RunAttack(matching_race);
    const int damage_without_bonus = RunAttack(other_race);

    REQUIRE(damage_with_bonus > damage_without_bonus);
}

TEST_CASE("WeaponAttackAction dispatches AfterDamageEvent to the actor on a landed hit, and on a kill",
          "[WeaponAttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10, /*player=*/false);
    const entt::entity enemy_handle = enemy.Handle();

    int damage_events = 0;
    bool saw_defeat = false;
    actor.Get<psr::EventHandlerComponent>().Subscribe<psr::AfterDamageEvent, DamageEventProbe>(
        [&](psr::Entity, psr::AfterDamageEvent& event)
        {
            ++damage_events;
            if (event.target_defeated)
                saw_defeat = true;
        });

    psr::WeaponAttackAction action(grid, affixes, rng, psr::Vec2{1, 0});
    for (int attempt = 0; attempt < 50 && registry.IsValid(enemy_handle); ++attempt)
    {
        action.Perform(actor);
        DrainAttackTween(registry);
    }

    REQUIRE_FALSE(registry.IsValid(enemy_handle));
    REQUIRE(damage_events > 0);
    REQUIRE(saw_defeat);
}

TEST_CASE("WeaponAttackAction is a free no-op when the weapon carrier lacks WeaponComponent", "[WeaponAttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/50, /*player=*/true);
    entt::entity not_a_weapon = registry.CreateEntity(); // no WeaponComponent
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{not_a_weapon});

    psr::WeaponAttackAction action(grid, affixes, rng, psr::Vec2{1, 0});
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("EquipmentComponent's handler contributes weapon data and effective stats to BeforeAttackEvent",
          "[WeaponAttackAction][EquipmentComponent]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/60, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry, psr::WeaponRangeShape::Cone3, /*range=*/2, /*hits_per_turn=*/3);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::BeforeAttackEvent event{psr::Vec2{1, 0}};
    actor.Dispatch(event);

    REQUIRE(event.has_weapon);
    REQUIRE(event.range_shape == psr::WeaponRangeShape::Cone3);
    REQUIRE(event.range == 2);
    REQUIRE(event.hits_per_turn == 3);
    REQUIRE(event.attacker_stats.atp == 50);
    REQUIRE(event.attacker_stats.ata == 60);
}

TEST_CASE("WeaponAttackAction no-ops for zero cost when the actor is Shocked", "[WeaponAttackAction][StatusEffect]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffect shock;
    shock.id = 1;
    shock.type = psr::StatusEffectType::Shock;
    shock.duration = 3;
    psr::StatusEffectLibrary status_effects{{shock}};
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    psr::ApplyStatusEffect(actor, status_effects, shock.id);

    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10, /*player=*/false);

    psr::WeaponAttackAction action(grid, affixes, rng, psr::Vec2{1, 0});
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE(enemy.Get<psr::HealthComponent>().current_hp == 10); // untouched -- the attack never happened
}

TEST_CASE("WeaponAttackAction applies the weapon's elemental status on a guaranteed-chance landed hit",
          "[WeaponAttackAction][StatusEffect]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffect burn;
    burn.id = 1;
    burn.type = psr::StatusEffectType::Burn;
    burn.magnitude = 2;
    burn.duration = 3;
    psr::StatusEffectLibrary status_effects{{burn}};
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry);
    psr::WeaponComponent& weapon_component = registry.GetComponent<psr::WeaponComponent>(weapon);
    weapon_component.element = psr::Element::Fire;
    weapon_component.status_effect_id = burn.id;
    weapon_component.status_chance_percent = 100;
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    // High HP so the hit lands but doesn't kill -- the elemental status hook
    // only rolls on a non-lethal hit.
    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10000, /*player=*/false);

    psr::WeaponAttackAction action(grid, affixes, rng, psr::Vec2{1, 0});
    action.Perform(actor);
    DrainAttackTween(registry);

    const psr::StatusEffectComponent* status = enemy.TryGet<psr::StatusEffectComponent>();
    REQUIRE(status != nullptr);
    REQUIRE_FALSE(status->active.empty());
    CHECK(status->active.front().status_effect_id == burn.id);
}

TEST_CASE("WeaponAttackAction knocks a landed-hit target back 1 tile away from the attacker", "[WeaponAttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    // High HP so the target survives to be pushed -- isolates knockback from
    // the death path.
    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10000, /*player=*/false);
    const entt::entity enemy_handle = enemy.Handle();

    psr::WeaponAttackAction action(grid, affixes, rng, psr::Vec2{1, 0});
    action.Perform(actor);
    DrainAttackTween(registry);

    REQUIRE(enemy.Get<psr::Position>().tile == psr::Vec2{3, 1});
    REQUIRE(grid.GetEntities({2, 1}).empty());
    REQUIRE_FALSE(grid.GetEntities({3, 1}).empty());
    REQUIRE(grid.GetEntities({3, 1}).front() == enemy_handle);
}

TEST_CASE("WeaponAttackAction does not knock a target into a blocked destination tile", "[WeaponAttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10000, /*player=*/false);

    // A wall on the tile the knockback would push into.
    entt::entity wall = registry.CreateEntity();
    registry.Emplace<psr::BlocksMovementComponent>(wall);
    grid.AddEntity({3, 1}, wall);

    psr::WeaponAttackAction action(grid, affixes, rng, psr::Vec2{1, 0});
    action.Perform(actor);
    DrainAttackTween(registry);

    REQUIRE(enemy.Get<psr::Position>().tile == psr::Vec2{2, 1}); // knockback silently skipped
}

TEST_CASE("WeaponAttackAction does not knock back a target with a zero KnockbackMultiplierComponent",
          "[WeaponAttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10000, /*player=*/false);
    enemy.Emplace<psr::KnockbackMultiplierComponent>(psr::KnockbackMultiplierComponent{0.0f});

    psr::WeaponAttackAction action(grid, affixes, rng, psr::Vec2{1, 0});
    action.Perform(actor);
    DrainAttackTween(registry);

    REQUIRE(enemy.Get<psr::Position>().tile == psr::Vec2{2, 1}); // knockback suppressed
}

TEST_CASE("WeaponAttackAction is a free no-op when a fixed-direction (bump) call targets a fires_projectile weapon",
          "[WeaponAttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry, psr::WeaponRangeShape::Line, /*range=*/5);
    registry.GetComponent<psr::WeaponComponent>(weapon).fires_projectile = true;
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10, /*player=*/false);

    // A fixed direction, as MoveAction's bump fallback always constructs.
    psr::WeaponAttackAction action(grid, affixes, rng, psr::Vec2{1, 0});
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE(enemy.Get<psr::HealthComponent>().current_hp == 10); // untouched -- bump never fires a ranged weapon
    REQUIRE_FALSE(registry.Any<psr::ProjectileComponent>()); // no projectile was ever spawned
}

TEST_CASE("WeaponAttackAction with no fixed direction resolves via SelectedTargetComponent", "[WeaponAttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    actor.Emplace<psr::SelectedTargetComponent>(psr::SelectedTargetComponent{psr::Vec2{2, 1}});

    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10, /*player=*/false);
    const entt::entity enemy_handle = enemy.Handle();

    // No fixed direction -- this is how GameplayLayer::TryActivateSlot's
    // hotbar Normal Attack slot invokes it, after TargetSelectionState has
    // already written SelectedTargetComponent.
    psr::WeaponAttackAction action(grid, affixes, rng);

    bool destroyed = false;
    for (int attempt = 0; attempt < 50 && !destroyed; ++attempt)
    {
        action.Perform(actor);
        DrainAttackTween(registry);
        if (!registry.IsValid(enemy_handle))
            destroyed = true;
    }

    REQUIRE(destroyed);
}

TEST_CASE("WeaponAttackAction resolves a diagonally-selected target instead of snapping it to a cardinal tile",
          "[WeaponAttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    // Diagonal offset {1,1} -- a cardinal-only snap would flatten this to
    // {1,0} (tile {2,1}), missing the defender placed at the true diagonal
    // neighbour {2,2}.
    actor.Emplace<psr::SelectedTargetComponent>(psr::SelectedTargetComponent{psr::Vec2{2, 2}});

    psr::Entity enemy = MakeDefender(registry, grid, {2, 2}, /*dfp=*/0, /*evp=*/0, /*hp=*/10, /*player=*/false);
    const entt::entity enemy_handle = enemy.Handle();

    psr::WeaponAttackAction action(grid, affixes, rng);

    bool destroyed = false;
    for (int attempt = 0; attempt < 50 && !destroyed; ++attempt)
    {
        action.Perform(actor);
        DrainAttackTween(registry);
        if (!registry.IsValid(enemy_handle))
            destroyed = true;
    }

    REQUIRE(destroyed);
}

TEST_CASE("WeaponAttackAction with no fixed direction and a fires_projectile weapon charges cost without a "
          "registered projectile prefab",
          "[WeaponAttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry, psr::WeaponRangeShape::Line, /*range=*/5);
    registry.GetComponent<psr::WeaponComponent>(weapon).fires_projectile = true;
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    actor.Emplace<psr::SelectedTargetComponent>(psr::SelectedTargetComponent{psr::Vec2{2, 1}});

    // No prefab registered for projectile_prefab_id (0, the default) --
    // registry.HasPrefab(0) is false, so no ProjectileComponent entity can
    // spawn, but the cast still commits (same "cost is charged once the gate
    // passes" convention TechniqueAction's own projectile branch uses).
    psr::WeaponAttackAction action(grid, affixes, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::WeaponAttackAction::kWeaponAttackCost);
    REQUIRE_FALSE(registry.Any<psr::ProjectileComponent>());
}
