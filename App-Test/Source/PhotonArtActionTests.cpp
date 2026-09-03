#include "Actions/PhotonArtAction.h"

#include "Combat/PhotonArtCastEvent.h"
#include "Combat/PhotonArtLibrary.h"
#include "Combat/StatusEffectApplication.h"
#include "CombatRegistrySetup.h"
#include "Components/EquipmentComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RaceComponent.h"
#include "Components/SelectedTargetComponent.h"
#include "Components/StatsComponent.h"
#include "Components/StatusEffectComponent.h"
#include "Components/TPComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr std::uint32_t kArtId = 1;

// Tag type identifying this test file's subscriptions to EventHandlerComponent
// -- never instantiated, only used as Subscribe/Unsubscribe's TOwner key.
struct PhotonArtEventProbe
{
};

// A weapon that grants kArtId, generous ATP/ATA so hits are guaranteed
// (ComputeHitChance's Accuracy = ATA - EVP*0.2 clamps to 1.0 once ATA alone
// exceeds 100), mirrors AttackActionTests.cpp's MakeWeapon.
entt::entity MakeWeapon(psr::Registry& registry, bool grants_art = true)
{
    entt::entity weapon = registry.CreateEntity();
    psr::WeaponComponent component;
    if (grants_art)
        component.photon_art_ids.push_back(kArtId);
    registry.Emplace<psr::WeaponComponent>(weapon, component);
    registry.Emplace<psr::StatsComponent>(weapon); // no weapon stat bonus needed for these tests
    return weapon;
}

psr::Entity MakeActor(psr::Registry& registry, psr::Grid& grid, psr::Vec2 tile, int atp, int ata, int tp)
{
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(tile);
    grid.AddEntity(tile, handle);
    psr::StatsComponent stats;
    stats.atp = atp;
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

psr::PhotonArtLibrary MakeLibrary(psr::PhotonArt art)
{
    art.id = kArtId;
    std::vector<psr::PhotonArt> arts;
    arts.push_back(std::move(art));
    return psr::PhotonArtLibrary{std::move(arts)};
}

} // namespace

TEST_CASE("PhotonArtAction with no weapon equipped is a free no-op", "[PhotonArtAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    psr::PhotonArtLibrary arts = MakeLibrary(psr::PhotonArt{});
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/50, /*tp=*/100);

    psr::PhotonArtAction action(grid, arts, affixes, kArtId, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("PhotonArtAction with a weapon that doesn't grant the id is a free no-op", "[PhotonArtAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    psr::PhotonArtLibrary arts = MakeLibrary(psr::PhotonArt{});
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/50, /*tp=*/100);
    entt::entity weapon = MakeWeapon(registry, /*grants_art=*/false);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::PhotonArtAction action(grid, arts, affixes, kArtId, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("PhotonArtAction with insufficient TP is a free no-op", "[PhotonArtAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    psr::PhotonArt art;
    art.tp_cost = 20;
    psr::PhotonArtLibrary arts = MakeLibrary(art);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/50, /*tp=*/10);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::PhotonArtAction action(grid, arts, affixes, kArtId, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE(actor.Get<psr::TPComponent>().current_tp == 10); // untouched
}

TEST_CASE("PhotonArtAction self-target (no SelectedTargetComponent) damages the caster with no hit roll",
          "[PhotonArtAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    psr::PhotonArt art;
    art.tp_cost = 5;
    art.effect_family = psr::EffectFamily::Damage;
    psr::PhotonArtLibrary arts = MakeLibrary(art);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/0, /*tp=*/10);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    psr::HealthComponent health;
    health.current_hp = 100;
    health.max_hp = 100;
    actor.Emplace<psr::HealthComponent>(health);

    // No SelectedTargetComponent -- offset defaults to {0,0} (self).
    psr::PhotonArtAction action(grid, arts, affixes, kArtId, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::PhotonArtAction::kPhotonArtCost);
    REQUIRE(actor.Get<psr::TPComponent>().current_tp == 5);
    REQUIRE(actor.Get<psr::HealthComponent>().current_hp < 100); // always hits itself, no roll to miss
}

TEST_CASE("PhotonArtAction self-target Drain heals the caster capped at max HP", "[PhotonArtAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    psr::PhotonArt art;
    art.tp_cost = 5;
    art.effect_family = psr::EffectFamily::Drain;
    art.drain_percent = 100;
    psr::PhotonArtLibrary arts = MakeLibrary(art);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/0, /*tp=*/10);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    psr::HealthComponent health;
    health.current_hp = 90;
    health.max_hp = 100;
    actor.Emplace<psr::HealthComponent>(health);

    psr::PhotonArtAction action(grid, arts, affixes, kArtId, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::PhotonArtAction::kPhotonArtCost);
    REQUIRE(actor.Get<psr::HealthComponent>().current_hp <= 100); // capped, never over max
    REQUIRE(actor.Get<psr::HealthComponent>().current_hp >= 90);  // a heal, not a drop
}

TEST_CASE("PhotonArtAction eventually destroys a hostile Directional-cast occupant", "[PhotonArtAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    psr::PhotonArt art;
    art.tp_cost = 0;
    art.targeting_mode = psr::TargetingMode::Directional;
    art.range_shape = psr::WeaponRangeShape::SingleTarget;
    art.range = 1;
    art.hits_per_turn = 1;
    art.effect_family = psr::EffectFamily::Damage;
    psr::PhotonArtLibrary arts = MakeLibrary(art);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*tp=*/100000);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    actor.Emplace<psr::SelectedTargetComponent>(psr::SelectedTargetComponent{psr::Vec2{2, 1}});

    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10);
    const entt::entity enemy_handle = enemy.Handle();

    psr::PhotonArtAction action(grid, arts, affixes, kArtId, rng);

    bool destroyed = false;
    for (int attempt = 0; attempt < 50 && !destroyed; ++attempt)
    {
        psr::ActionResult result = action.Perform(actor);
        REQUIRE(result.cost == psr::PhotonArtAction::kPhotonArtCost);
        if (!registry.IsValid(enemy_handle))
            destroyed = true;
    }

    REQUIRE(destroyed);
}

TEST_CASE("PhotonArtAction applies a tier power multiplier", "[PhotonArtAction]")
{
    psr::AffixLibrary affixes;

    auto RunCast = [&](float multiplier) -> int
    {
        psr::Registry registry;
        psr::Grid grid{5, 5};
        psr::StatusEffectLibrary status_effects;
        psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
        std::mt19937 rng{42};

        psr::PhotonArt art;
        art.tp_cost = 0;
        art.range_shape = psr::WeaponRangeShape::SingleTarget;
        art.range = 1;
        art.hits_per_turn = 1;
        art.effect_family = psr::EffectFamily::Damage;
        if (multiplier != 1.0f)
            art.tiers.push_back(psr::PhotonArtTier{2, multiplier});
        psr::PhotonArtLibrary arts = MakeLibrary(art);

        psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*tp=*/100000);
        entt::entity weapon = MakeWeapon(registry);
        actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
        actor.Emplace<psr::SelectedTargetComponent>(psr::SelectedTargetComponent{psr::Vec2{2, 1}});

        psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/100000);

        psr::PhotonArtAction action(grid, arts, affixes, kArtId, rng);
        action.Perform(actor);
        return 100000 - enemy.Get<psr::HealthComponent>().current_hp;
    };

    const int base_damage = RunCast(1.0f);
    const int boosted_damage = RunCast(2.0f);

    // Same rng seed and call sequence -- the only difference is the tier
    // multiplier, so the hit/miss outcome is identical between runs.
    REQUIRE(boosted_damage > base_damage);
}

TEST_CASE("PhotonArtAction dispatches AfterPhotonArtCastEvent and AfterDamageEvent on a self-target Damage cast",
          "[PhotonArtAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    psr::PhotonArt art;
    art.tp_cost = 5;
    art.effect_family = psr::EffectFamily::Damage;
    psr::PhotonArtLibrary arts = MakeLibrary(art);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/0, /*tp=*/10);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    psr::HealthComponent health;
    health.current_hp = 100;
    health.max_hp = 100;
    actor.Emplace<psr::HealthComponent>(health);

    int cast_events = 0;
    int damage_events = 0;
    actor.Get<psr::EventHandlerComponent>().Subscribe<psr::AfterPhotonArtCastEvent, PhotonArtEventProbe>(
        [&](psr::Entity, psr::AfterPhotonArtCastEvent& event)
        {
            ++cast_events;
            REQUIRE(event.photon_art_id == kArtId);
        });
    actor.Get<psr::EventHandlerComponent>().Subscribe<psr::AfterDamageEvent, PhotonArtEventProbe>(
        [&](psr::Entity, psr::AfterDamageEvent&) { ++damage_events; });

    psr::PhotonArtAction action(grid, arts, affixes, kArtId, rng);
    action.Perform(actor);

    REQUIRE(cast_events == 1);
    REQUIRE(damage_events == 1);
}

TEST_CASE("PhotonArtAction self-target Drain (a pure heal) dispatches no AfterDamageEvent", "[PhotonArtAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    psr::PhotonArt art;
    art.tp_cost = 5;
    art.effect_family = psr::EffectFamily::Drain;
    art.drain_percent = 100;
    psr::PhotonArtLibrary arts = MakeLibrary(art);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/0, /*tp=*/10);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    psr::HealthComponent health;
    health.current_hp = 90;
    health.max_hp = 100;
    actor.Emplace<psr::HealthComponent>(health);

    int damage_events = 0;
    actor.Get<psr::EventHandlerComponent>().Subscribe<psr::AfterDamageEvent, PhotonArtEventProbe>(
        [&](psr::Entity, psr::AfterDamageEvent&) { ++damage_events; });

    psr::PhotonArtAction action(grid, arts, affixes, kArtId, rng);
    action.Perform(actor);

    REQUIRE(damage_events == 0);
}

TEST_CASE("EquipmentComponent's handler contributes weapon-grants and stats, TPComponent's contributes current_tp, "
          "to BeforePhotonArtCastEvent",
          "[PhotonArtAction][EquipmentComponent][TPComponent]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/65, /*ata=*/45, /*tp=*/12);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::BeforePhotonArtCastEvent event{kArtId};
    actor.Dispatch(event);

    REQUIRE(event.has_weapon);
    REQUIRE(event.weapon_grants_id);
    REQUIRE(event.has_tp_component);
    REQUIRE(event.current_tp == 12);
    REQUIRE(event.attacker_stats.atp == 65);
    REQUIRE(event.attacker_stats.ata == 45);
}

TEST_CASE("PhotonArtAction no-ops for zero cost when the caster is Shocked", "[PhotonArtAction][StatusEffect]")
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
    psr::PhotonArt art;
    art.tp_cost = 0;
    art.effect_family = psr::EffectFamily::Damage;
    psr::PhotonArtLibrary arts = MakeLibrary(art);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*tp=*/100);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    psr::ApplyStatusEffect(actor, status_effects, shock.id);

    psr::PhotonArtAction action(grid, arts, affixes, kArtId, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE(actor.Get<psr::TPComponent>().current_tp == 100); // untouched -- the cast never happened
}

TEST_CASE("PhotonArtAction applies the wielded weapon's elemental status on a guaranteed-chance landed hit",
          "[PhotonArtAction][StatusEffect]")
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
    psr::PhotonArt art;
    art.tp_cost = 0;
    art.targeting_mode = psr::TargetingMode::Directional;
    art.range_shape = psr::WeaponRangeShape::SingleTarget;
    art.range = 1;
    art.hits_per_turn = 1;
    art.effect_family = psr::EffectFamily::Damage;
    psr::PhotonArtLibrary arts = MakeLibrary(art);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*tp=*/100000);
    entt::entity weapon = MakeWeapon(registry);
    psr::WeaponComponent& weapon_component = registry.GetComponent<psr::WeaponComponent>(weapon);
    weapon_component.element = psr::Element::Fire;
    weapon_component.status_effect_id = burn.id;
    weapon_component.status_chance_percent = 100;
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    actor.Emplace<psr::SelectedTargetComponent>(psr::SelectedTargetComponent{psr::Vec2{2, 1}});

    // High HP so the hit lands but doesn't kill.
    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10000);

    psr::PhotonArtAction action(grid, arts, affixes, kArtId, rng);
    action.Perform(actor);

    const psr::StatusEffectComponent* status = enemy.TryGet<psr::StatusEffectComponent>();
    REQUIRE(status != nullptr);
    REQUIRE_FALSE(status->active.empty());
    CHECK(status->active.front().status_effect_id == burn.id);
}

TEST_CASE("PhotonArtAction EffectFamily::Status applies the ailment on hit and deals no damage",
          "[PhotonArtAction][StatusEffect]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffect poison;
    poison.id = 1;
    poison.type = psr::StatusEffectType::Poison;
    poison.magnitude = 2;
    poison.duration = 3;
    psr::StatusEffectLibrary status_effects{{poison}};
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);
    psr::PhotonArt art;
    art.tp_cost = 0;
    art.targeting_mode = psr::TargetingMode::Directional;
    art.range_shape = psr::WeaponRangeShape::SingleTarget;
    art.range = 1;
    art.hits_per_turn = 1;
    art.effect_family = psr::EffectFamily::Status;
    art.status_effect_id = poison.id;
    psr::PhotonArtLibrary arts = MakeLibrary(art);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*tp=*/100000);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});
    actor.Emplace<psr::SelectedTargetComponent>(psr::SelectedTargetComponent{psr::Vec2{2, 1}});

    psr::Entity enemy = MakeDefender(registry, grid, {2, 1}, /*dfp=*/0, /*evp=*/0, /*hp=*/10);

    psr::PhotonArtAction action(grid, arts, affixes, kArtId, rng);
    action.Perform(actor);

    CHECK(enemy.Get<psr::HealthComponent>().current_hp == 10); // Status family deals no damage
    const psr::StatusEffectComponent* status = enemy.TryGet<psr::StatusEffectComponent>();
    REQUIRE(status != nullptr);
    REQUIRE_FALSE(status->active.empty());
    CHECK(status->active.front().status_effect_id == poison.id);
}
