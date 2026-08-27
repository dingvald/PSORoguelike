#include "Actions/PhotonArtAction.h"

#include "Components/EquipmentComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/SelectedTargetComponent.h"
#include "Engine/Combat/PhotonArtLibrary.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/PPComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/RaceComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/StatsComponent.h"
#include "Engine/ECS/WeaponComponent.h"

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr std::uint32_t kArtId = 1;

// A weapon that grants kArtId, generous ATP/ATA so hits are overwhelmingly
// likely (though never guaranteed -- ComputeHitChance clamps at 0.95),
// mirrors AttackActionTests.cpp's MakeWeapon.
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

psr::Entity MakeActor(psr::Registry& registry, psr::Grid& grid, psr::Vec2 tile, int atp, int ata, int pp)
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
    psr::PPComponent pp_component;
    pp_component.current_pp = pp;
    pp_component.max_pp = pp;
    actor.Emplace<psr::PPComponent>(pp_component);
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
    psr::PhotonArtLibrary arts = MakeLibrary(psr::PhotonArt{});
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/50, /*pp=*/100);

    psr::PhotonArtAction action(grid, arts, affixes, kArtId, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("PhotonArtAction with a weapon that doesn't grant the id is a free no-op", "[PhotonArtAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::PhotonArtLibrary arts = MakeLibrary(psr::PhotonArt{});
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/50, /*pp=*/100);
    entt::entity weapon = MakeWeapon(registry, /*grants_art=*/false);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::PhotonArtAction action(grid, arts, affixes, kArtId, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("PhotonArtAction with insufficient PP is a free no-op", "[PhotonArtAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::PhotonArt art;
    art.pp_cost = 20;
    psr::PhotonArtLibrary arts = MakeLibrary(art);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/50, /*pp=*/10);
    entt::entity weapon = MakeWeapon(registry);
    actor.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{weapon});

    psr::PhotonArtAction action(grid, arts, affixes, kArtId, rng);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE(actor.Get<psr::PPComponent>().current_pp == 10); // untouched
}

TEST_CASE("PhotonArtAction self-target (no SelectedTargetComponent) damages the caster with no hit roll",
          "[PhotonArtAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::PhotonArt art;
    art.pp_cost = 5;
    art.effect_family = psr::EffectFamily::Damage;
    psr::PhotonArtLibrary arts = MakeLibrary(art);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/0, /*pp=*/10);
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
    REQUIRE(actor.Get<psr::PPComponent>().current_pp == 5);
    REQUIRE(actor.Get<psr::HealthComponent>().current_hp < 100); // always hits itself, no roll to miss
}

TEST_CASE("PhotonArtAction self-target Drain heals the caster capped at max HP", "[PhotonArtAction]")
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::PhotonArt art;
    art.pp_cost = 5;
    art.effect_family = psr::EffectFamily::Drain;
    art.drain_percent = 100;
    psr::PhotonArtLibrary arts = MakeLibrary(art);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/50, /*ata=*/0, /*pp=*/10);
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
    psr::PhotonArt art;
    art.pp_cost = 0;
    art.targeting_mode = psr::TargetingMode::Directional;
    art.range_shape = psr::WeaponRangeShape::SingleTarget;
    art.range = 1;
    art.hits_per_turn = 1;
    art.effect_family = psr::EffectFamily::Damage;
    psr::PhotonArtLibrary arts = MakeLibrary(art);
    std::mt19937 rng{1};

    psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*pp=*/100000);
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
        std::mt19937 rng{42};

        psr::PhotonArt art;
        art.pp_cost = 0;
        art.range_shape = psr::WeaponRangeShape::SingleTarget;
        art.range = 1;
        art.hits_per_turn = 1;
        art.effect_family = psr::EffectFamily::Damage;
        if (multiplier != 1.0f)
            art.tiers.push_back(psr::PhotonArtTier{2, multiplier});
        psr::PhotonArtLibrary arts = MakeLibrary(art);

        psr::Entity actor = MakeActor(registry, grid, {1, 1}, /*atp=*/80, /*ata=*/200, /*pp=*/100000);
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
