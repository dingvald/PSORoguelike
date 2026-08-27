#include "Actions/TechniqueAction.h"

#include "CombatRegistrySetup.h"
#include "Components/EquipmentComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/SelectedTargetComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/Combat/TechniqueCastEvent.h"
#include "Engine/Combat/TechniqueLibrary.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/RaceComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/StatsComponent.h"
#include "Engine/ECS/TPComponent.h"
#include "Engine/ECS/WeaponComponent.h"

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr std::uint32_t kTechniqueId = 1;

// Tag type identifying this test file's subscriptions to EventHandlerComponent
// -- never instantiated, only used as Subscribe/Unsubscribe's TOwner key.
struct TechniqueEventProbe
{
};

entt::entity MakeWeapon(psr::Registry& registry, bool grants_technique = true)
{
    entt::entity weapon = registry.CreateEntity();
    psr::WeaponComponent component;
    if (grants_technique)
        component.technique_ids.push_back(kTechniqueId);
    registry.Emplace<psr::WeaponComponent>(weapon, component);
    registry.Emplace<psr::StatsComponent>(weapon); // no weapon stat bonus needed for these tests
    return weapon;
}

psr::Entity MakeActor(psr::Registry& registry, psr::Grid& grid, psr::Vec2 tile, int mst, int ata, int tp)
{
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(tile);
    grid.AddEntity(tile, handle);
    psr::StatsComponent stats;
    stats.mst = mst;
    stats.ata = ata;
    actor.Emplace<psr::StatsComponent>(stats);
    actor.Emplace<psr::PlayerControlledComponent>();
    psr::TPComponent tp_component;
    tp_component.current_tp = tp;
    tp_component.max_tp = tp;
    actor.Emplace<psr::TPComponent>(tp_component);
    return actor;
}

psr::Entity MakeDefender(psr::Registry& registry, psr::Grid& grid, psr::Vec2 tile, int dfp, int evp, int hp)
{
    entt::entity handle = registry.CreateEntity();
    psr::Entity defender(registry, handle);
    defender.Emplace<psr::Position>(tile);
    grid.AddEntity(tile, handle);
    psr::StatsComponent stats;
    stats.dfp = dfp;
    stats.evp = evp;
    defender.Emplace<psr::StatsComponent>(stats);
    psr::HealthComponent health;
    health.current_hp = hp;
    health.max_hp = hp;
    defender.Emplace<psr::HealthComponent>(health);
    return defender;
}

psr::TechniqueLibrary MakeLibrary(psr::Technique technique)
{
    technique.id = kTechniqueId;
    std::vector<psr::Technique> techniques;
    techniques.push_back(std::move(technique));
    return psr::TechniqueLibrary{std::move(techniques)};
}

} // namespace

TEST_CASE("TechniqueAction with no weapon equipped is a free no-op", "[TechniqueAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);
    psr::TechniqueLibrary techniques = MakeLibrary(psr::Technique{});
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*mst=*/50, /*ata=*/50, /*tp=*/100);

    psr::TechniqueAction action(grid, techniques, affixes, kTechniqueId, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("TechniqueAction with a weapon that doesn't grant the id is a free no-op", "[TechniqueAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);
    psr::TechniqueLibrary techniques = MakeLibrary(psr::Technique{});
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*mst=*/50, /*ata=*/50, /*tp=*/100);
    entt::entity weapon = MakeWeapon(registry, /*grants_technique=*/false);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::TechniqueAction action(grid, techniques, affixes, kTechniqueId, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("TechniqueAction with insufficient TP is a free no-op", "[TechniqueAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);
    psr::Technique technique;
    technique.tp_cost = 20;
    psr::TechniqueLibrary techniques = MakeLibrary(technique);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*mst=*/50, /*ata=*/50, /*tp=*/10);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::TechniqueAction action(grid, techniques, affixes, kTechniqueId, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE(actor.Get<psr::TPComponent>().current_tp == 10); // untouched
}

TEST_CASE("TechniqueAction self-target (no SelectedTargetComponent) damages the caster with no hit roll",
          "[TechniqueAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);
    psr::Technique technique;
    technique.tp_cost = 5;
    technique.effect_family = psr::EffectFamily::Damage;
    psr::TechniqueLibrary techniques = MakeLibrary(technique);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*mst=*/50, /*ata=*/0, /*tp=*/10);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    psr::HealthComponent health;
    health.current_hp = 100;
    health.max_hp = 100;
    actor.Emplace<psr::HealthComponent>(health);

    // No SelectedTargetComponent -- offset defaults to {0,0} (self).
    psr::TechniqueAction action(grid, techniques, affixes, kTechniqueId, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::TechniqueAction::kTechniqueCost);
    REQUIRE(actor.Get<psr::TPComponent>().current_tp == 5);
    REQUIRE(actor.Get<psr::HealthComponent>().current_hp < 100); // always hits itself, no roll to miss
}

TEST_CASE("TechniqueAction self-target Drain resolves identically to Damage (no drain_percent field)",
          "[TechniqueAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);
    psr::Technique technique;
    technique.tp_cost = 5;
    technique.effect_family = psr::EffectFamily::Drain;
    psr::TechniqueLibrary techniques = MakeLibrary(technique);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*mst=*/50, /*ata=*/0, /*tp=*/10);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    psr::HealthComponent health;
    health.current_hp = 100;
    health.max_hp = 100;
    actor.Emplace<psr::HealthComponent>(health);

    psr::TechniqueAction action(grid, techniques, affixes, kTechniqueId, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::TechniqueAction::kTechniqueCost);
    // Drain has no amount to size a restore by on Technique -- self-target
    // only special-cases Damage, so Drain is simply a no-effect turn spend.
    REQUIRE(actor.Get<psr::HealthComponent>().current_hp == 100);
}

TEST_CASE("TechniqueAction eventually destroys a hostile Directional-cast occupant", "[TechniqueAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);
    psr::Technique technique;
    technique.tp_cost = 0;
    technique.targeting_mode = psr::TargetingMode::Directional;
    technique.range_shape = psr::WeaponRangeShape::SingleTarget;
    technique.range = 1;
    technique.effect_family = psr::EffectFamily::Damage;
    psr::TechniqueLibrary techniques = MakeLibrary(technique);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*mst=*/80, /*ata=*/200, /*tp=*/100000);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    actor.Emplace<psr::SelectedTargetComponent>(psr::SelectedTargetComponent{psr::Vec2{2, 1}});

    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10);
    const entt::entity enemy_handle = enemy.Handle();

    psr::TechniqueAction action(grid, techniques, affixes, kTechniqueId, rng);

    bool destroyed = false;
    for (int attempt = 0; attempt < 50 && !destroyed; ++attempt)
    {
        psr::ActionResult result = action.Perform(actor);
        REQUIRE(result.cost == psr::TechniqueAction::kTechniqueCost);
        if (!registry.IsValid(enemy_handle))
            destroyed = true;
    }

    REQUIRE(destroyed);
}

TEST_CASE("TechniqueAction applies a tier power multiplier", "[TechniqueAction]")
{
    psr::AffixLibrary affixes;

    auto RunCast = [&](float multiplier) -> int
    {
        psr::Registry registry;
        psr::Grid grid{5, 5};
        psr::SetUpCombatRegistry(registry, affixes);
        std::mt19937 rng{42};

        psr::Technique technique;
        technique.tp_cost = 0;
        technique.range_shape = psr::WeaponRangeShape::SingleTarget;
        technique.range = 1;
        technique.effect_family = psr::EffectFamily::Damage;
        if (multiplier != 1.0f)
            technique.tiers.push_back(psr::TechniqueTier{2, multiplier});
        psr::TechniqueLibrary techniques = MakeLibrary(technique);

        psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*mst=*/80, /*ata=*/200, /*tp=*/100000);
        entt::entity weapon = MakeWeapon(registry);
        actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
        actor.Emplace<psr::SelectedTargetComponent>(psr::SelectedTargetComponent{psr::Vec2{2, 1}});

        psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/100000);

        psr::TechniqueAction action(grid, techniques, affixes, kTechniqueId, rng);
        action.Perform(actor);
        return 100000 - enemy.Get<psr::HealthComponent>().current_hp;
    };

    const int base_damage = RunCast(1.0f);
    const int boosted_damage = RunCast(2.0f);

    // Same rng seed and call sequence -- the only difference is the tier
    // multiplier, so the hit/miss outcome is identical between runs.
    REQUIRE(boosted_damage > base_damage);
}

TEST_CASE("TechniqueAction applies a matching race bonus", "[TechniqueAction]")
{
    psr::AffixLibrary affixes;
    const std::uint32_t matching_race = 111;
    const std::uint32_t other_race = 222;

    auto RunCast = [&](std::uint32_t defender_race) -> int
    {
        psr::Registry registry;
        psr::Grid grid{5, 5};
        psr::SetUpCombatRegistry(registry, affixes);
        std::mt19937 rng{42};

        psr::Technique technique;
        technique.tp_cost = 0;
        technique.range_shape = psr::WeaponRangeShape::SingleTarget;
        technique.range = 1;
        technique.effect_family = psr::EffectFamily::Damage;
        psr::TechniqueLibrary techniques = MakeLibrary(technique);

        psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*mst=*/80, /*ata=*/200, /*tp=*/100000);
        entt::entity weapon = MakeWeapon(registry);
        psr::WeaponComponent& weapon_component = registry.GetComponent<psr::WeaponComponent>(weapon);
        weapon_component.race_bonuses.push_back({matching_race, /*bonus_percent=*/50});
        actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
        actor.Emplace<psr::SelectedTargetComponent>(psr::SelectedTargetComponent{psr::Vec2{2, 1}});

        psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/100000);
        enemy.Emplace<psr::RaceComponent>(psr::RaceComponent{defender_race});

        psr::TechniqueAction action(grid, techniques, affixes, kTechniqueId, rng);
        action.Perform(actor);
        return 100000 - enemy.Get<psr::HealthComponent>().current_hp;
    };

    const int damage_with_bonus = RunCast(matching_race);
    const int damage_without_bonus = RunCast(other_race);

    REQUIRE(damage_with_bonus > damage_without_bonus);
}

TEST_CASE("TechniqueAction dispatches AfterTechniqueCastEvent and AfterDamageEvent on a self-target cast",
          "[TechniqueAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);
    psr::Technique technique;
    technique.tp_cost = 5;
    technique.effect_family = psr::EffectFamily::Damage;
    psr::TechniqueLibrary techniques = MakeLibrary(technique);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*mst=*/50, /*ata=*/0, /*tp=*/10);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    psr::HealthComponent health;
    health.current_hp = 100;
    health.max_hp = 100;
    actor.Emplace<psr::HealthComponent>(health);

    int cast_events = 0;
    int damage_events = 0;
    actor.Get<psr::EventHandlerComponent>().Subscribe<psr::AfterTechniqueCastEvent, TechniqueEventProbe>(
        [&](psr::Entity, psr::AfterTechniqueCastEvent& event)
        {
            ++cast_events;
            REQUIRE(event.technique_id == kTechniqueId);
        });
    actor.Get<psr::EventHandlerComponent>().Subscribe<psr::AfterDamageEvent, TechniqueEventProbe>(
        [&](psr::Entity, psr::AfterDamageEvent&) { ++damage_events; });

    psr::TechniqueAction action(grid, techniques, affixes, kTechniqueId, rng);
    action.Perform(actor);

    REQUIRE(cast_events == 1);
    REQUIRE(damage_events == 1);
}

TEST_CASE("EquipmentComponent's handler contributes weapon-grants and stats, TPComponent's contributes current_tp, "
          "to BeforeTechniqueCastEvent",
          "[TechniqueAction][EquipmentComponent][TPComponent]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*mst=*/70, /*ata=*/40, /*tp=*/33);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::BeforeTechniqueCastEvent event{kTechniqueId};
    actor.Dispatch(event);

    REQUIRE(event.has_weapon);
    REQUIRE(event.weapon_grants_id);
    REQUIRE(event.has_tp_component);
    REQUIRE(event.current_tp == 33);
    REQUIRE(event.attacker_stats.mst == 70);
    REQUIRE(event.attacker_stats.ata == 40);
}

TEST_CASE("BeforeTechniqueCastEvent reports weapon_grants_id false for an id the weapon doesn't grant",
          "[TechniqueAction][EquipmentComponent]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::SetUpCombatRegistry(registry, affixes);

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*mst=*/50, /*ata=*/50, /*tp=*/10);
    entt::entity weapon = MakeWeapon(registry, /*grants_technique=*/false);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::BeforeTechniqueCastEvent event{kTechniqueId};
    actor.Dispatch(event);

    REQUIRE(event.has_weapon);
    REQUIRE_FALSE(event.weapon_grants_id);
}
