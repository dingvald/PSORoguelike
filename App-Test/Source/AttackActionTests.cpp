#include "Actions/AttackAction.h"

#include "CombatRegistrySetup.h"
#include "Components/EquipmentComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Engine/Combat/AttackEvent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/RaceComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/StatsComponent.h"
#include "Engine/ECS/WeaponComponent.h"

#include <catch2/catch_test_macros.hpp>

namespace {

// Tag type identifying this test file's subscriptions to EventHandlerComponent
// -- never instantiated, only used as Subscribe/Unsubscribe's TOwner key.
struct DamageEventProbe
{
};

// A weapon with generous ATP/ATA so hits are overwhelmingly likely (though
// never guaranteed -- ComputeHitChance clamps at 0.95) and damage is well
// above 1, keeping the fixed-seed tests below non-flaky in practice without
// hand-verifying individual RNG draws.
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

TEST_CASE("AttackAction with no weapon equipped is a free no-op", "[AttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/50, /*player=*/true);

    psr::AttackAction action(grid, affixes, psr::Vec2{1, 0}, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("AttackAction against an empty tile is a free no-op", "[AttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/50, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::AttackAction action(grid, affixes, psr::Vec2{1, 0}, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("AttackAction does not damage a non-hostile occupant", "[AttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/50, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    // Same side (both player-controlled) -- not hostile per IsHostile.
    psr::Entity ally = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/20, /*player=*/true);

    psr::AttackAction action(grid, affixes, psr::Vec2{1, 0}, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE(ally.Get<psr::HealthComponent>().current_hp == 20);
}

TEST_CASE("AttackAction eventually destroys a hostile SingleTarget occupant", "[AttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10, /*player=*/false);
    const entt::entity enemy_handle = enemy.Handle();

    psr::AttackAction action(grid, affixes, psr::Vec2{1, 0}, rng);

    bool destroyed = false;
    for (int attempt = 0; attempt < 50 && !destroyed; ++attempt)
    {
        psr::ActionResult result = action.Perform(actor);
        REQUIRE(result.cost == psr::AttackAction::kAttackCost); // a hostile target was always found in range
        if (!registry.IsValid(enemy_handle))
            destroyed = true;
    }

    REQUIRE(destroyed);
    REQUIRE(grid.GetEntities({2, 1}).empty());
}

TEST_CASE("AttackAction with hits_per_turn > 1 rolls multiple hits per Perform", "[AttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);
    std::mt19937 rng{7};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
    entt::entity weapon = MakeWeapon(registry, psr::WeaponRangeShape::SingleTarget, /*range=*/1, /*hits_per_turn=*/5);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    // High HP so a single hit_per_turn iteration can't kill it outright --
    // isolates "multiple hits landed in one Perform" from the death path.
    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10000, /*player=*/false);

    psr::AttackAction action(grid, affixes, psr::Vec2{1, 0}, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::AttackAction::kAttackCost);
    // ATP 80 vs DFP 0 lands well above 1 damage per hit; five overwhelmingly-
    // likely hits should clear far more than what any single hit could deal.
    const int damage_dealt = 10000 - enemy.Get<psr::HealthComponent>().current_hp;
    REQUIRE(damage_dealt > 80);
}

TEST_CASE("AttackAction applies a matching race bonus", "[AttackAction]")
{
    psr::AffixLibrary affixes;
    const std::uint32_t matching_race = 111;
    const std::uint32_t other_race = 222;

    auto RunAttack = [&](std::uint32_t defender_race) -> int
    {
        psr::Registry registry;
        psr::Grid grid{5, 5};
        psr::SetUpCombatRegistry(registry, affixes);
        std::mt19937 rng{42};

        psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*player=*/true);
        entt::entity weapon = MakeWeapon(registry);
        psr::WeaponComponent& weapon_component = registry.GetComponent<psr::WeaponComponent>(weapon);
        weapon_component.race_bonuses.push_back({matching_race, /*bonus_percent=*/50});
        actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

        psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10000, /*player=*/false);
        enemy.Emplace<psr::RaceComponent>(psr::RaceComponent{defender_race});

        psr::AttackAction action(grid, affixes, psr::Vec2{1, 0}, rng);
        action.Perform(actor);
        return 10000 - enemy.Get<psr::HealthComponent>().current_hp;
    };

    const int damage_with_bonus = RunAttack(matching_race);
    const int damage_without_bonus = RunAttack(other_race);

    REQUIRE(damage_with_bonus > damage_without_bonus);
}

TEST_CASE("AttackAction dispatches AfterDamageEvent to the actor on a landed hit, and on a kill", "[AttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);
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

    psr::AttackAction action(grid, affixes, psr::Vec2{1, 0}, rng);
    for (int attempt = 0; attempt < 50 && registry.IsValid(enemy_handle); ++attempt)
        action.Perform(actor);

    REQUIRE_FALSE(registry.IsValid(enemy_handle));
    REQUIRE(damage_events > 0);
    REQUIRE(saw_defeat);
}

TEST_CASE("AttackAction is a free no-op when the weapon carrier lacks WeaponComponent", "[AttackAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/50, /*player=*/true);
    entt::entity not_a_weapon = registry.CreateEntity(); // no WeaponComponent
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{not_a_weapon});

    psr::AttackAction action(grid, affixes, psr::Vec2{1, 0}, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("EquipmentComponent's handler contributes weapon data and effective stats to BeforeAttackEvent",
          "[AttackAction][EquipmentComponent]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);
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
