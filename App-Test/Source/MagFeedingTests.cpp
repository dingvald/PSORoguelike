#include "Items/Mag/MagFeeding.h"

#include "Components/MagComponent.h"
#include "Components/RenderableComponent.h"
#include "Engine/ECS/IEntityLoader.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <unordered_map>

namespace {

constexpr std::uint32_t kSpeciesAPrefabId = 1;
constexpr std::uint32_t kSpeciesBPrefabId = 2;

// Two mag-species prefab templates: A's evolution_tree evolves into B once
// POW reaches 3, B has a different feed_cooldown_turns/RenderableComponent
// so a test can tell the swap actually happened. Mirrors
// Core-Test/RegistryPrefabTests.cpp's TwoPrefabsEntityLoader shape --
// CopyFromPrefab/GetPrefabComponent don't go through entt::meta (see
// Registry.h), so no ComponentSchemaRegistrar registration is needed here.
class MagSpeciesEntityLoader : public psr::IEntityLoader
{
public:
    bool Load(std::filesystem::path /*path*/) override { return true; }

    void Populate(entt::registry& prefab_registry,
                  std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
    {
        entt::entity species_a = prefab_registry.create();
        psr::MagComponent a;
        a.feed_cooldown_turns = 50;
        a.feed_response.push_back({/*item_prefab_id=*/100, /*pow=*/101, /*def=*/0, /*dex=*/0, /*mind=*/0});
        a.evolution_tree.push_back({"POW >= 3", kSpeciesBPrefabId});
        prefab_registry.emplace<psr::MagComponent>(species_a, a);
        psr::RenderableComponent renderable_a;
        renderable_a.texture_id = 111;
        prefab_registry.emplace<psr::RenderableComponent>(species_a, renderable_a);
        out_prefab_ids.emplace(kSpeciesAPrefabId, species_a);

        entt::entity species_b = prefab_registry.create();
        psr::MagComponent b;
        b.feed_cooldown_turns = 80;
        prefab_registry.emplace<psr::MagComponent>(species_b, b);
        psr::RenderableComponent renderable_b;
        renderable_b.texture_id = 222;
        prefab_registry.emplace<psr::RenderableComponent>(species_b, renderable_b);
        out_prefab_ids.emplace(kSpeciesBPrefabId, species_b);
    }
};

constexpr std::uint32_t kSelfLoopPrefabId = 1;

// A single species whose own evolution rule targets itself -- the shipped
// mags.mag content's own shape (see App/Assets/Data/Entities/mags/mag.json),
// proving the full evolve path (condition eval -> CopyFromPrefab -> re-
// lookup) is a correct no-op.
class SelfLoopEntityLoader : public psr::IEntityLoader
{
public:
    bool Load(std::filesystem::path /*path*/) override { return true; }

    void Populate(entt::registry& prefab_registry,
                  std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
    {
        entt::entity species = prefab_registry.create();
        psr::MagComponent component;
        component.feed_cooldown_turns = 50;
        component.feed_response.push_back({/*item_prefab_id=*/7, /*pow=*/100, /*def=*/0, /*dex=*/0, /*mind=*/0});
        component.evolution_tree.push_back({"LEVEL >= 1", kSelfLoopPrefabId});
        prefab_registry.emplace<psr::MagComponent>(species, component);
        prefab_registry.emplace<psr::RenderableComponent>(species);
        out_prefab_ids.emplace(kSelfLoopPrefabId, species);
    }
};

} // namespace

TEST_CASE("ApplyMagFood adds progress without yet reaching a level", "[MagFeeding]")
{
    psr::Registry registry;
    entt::entity mag = registry.CreateEntity();
    psr::MagComponent component;
    component.feed_response.push_back({/*item_prefab_id=*/7, /*pow=*/3, /*def=*/0, /*dex=*/0, /*mind=*/0});
    registry.Emplace<psr::MagComponent>(mag, component);

    REQUIRE(psr::ApplyMagFood(registry, mag, 7));

    const psr::MagComponent& result = registry.GetComponent<psr::MagComponent>(mag);
    CHECK(result.pow_level == 0);
    CHECK(result.pow_progress == 3);
}

TEST_CASE("ApplyMagFood rolls progress into multiple stat levels in one feed", "[MagFeeding]")
{
    psr::Registry registry;
    entt::entity mag = registry.CreateEntity();
    psr::MagComponent component;
    // 212 / kMagPointsPerLevel(100) = 2 levels, remainder 12.
    component.feed_response.push_back({/*item_prefab_id=*/7, /*pow=*/212, /*def=*/0, /*dex=*/0, /*mind=*/0});
    registry.Emplace<psr::MagComponent>(mag, component);

    REQUIRE(psr::ApplyMagFood(registry, mag, 7));

    const psr::MagComponent& result = registry.GetComponent<psr::MagComponent>(mag);
    CHECK(result.pow_level == 2);
    CHECK(result.pow_progress == 12);
}

TEST_CASE("ApplyMagFood clamps a negative delta at zero, never going below", "[MagFeeding]")
{
    psr::Registry registry;
    entt::entity mag = registry.CreateEntity();
    psr::MagComponent component;
    component.pow_level = 0;
    component.pow_progress = 1;
    component.feed_response.push_back({/*item_prefab_id=*/7, /*pow=*/-10, /*def=*/0, /*dex=*/0, /*mind=*/0});
    registry.Emplace<psr::MagComponent>(mag, component);

    REQUIRE(psr::ApplyMagFood(registry, mag, 7));

    const psr::MagComponent& result = registry.GetComponent<psr::MagComponent>(mag);
    CHECK(result.pow_level == 0);
    CHECK(result.pow_progress == 0);
}

TEST_CASE("ApplyMagFood is a no-op for an item prefab not in feed_response", "[MagFeeding]")
{
    psr::Registry registry;
    entt::entity mag = registry.CreateEntity();
    psr::MagComponent component;
    component.feed_response.push_back({/*item_prefab_id=*/7, /*pow=*/5, /*def=*/0, /*dex=*/0, /*mind=*/0});
    registry.Emplace<psr::MagComponent>(mag, component);

    REQUIRE_FALSE(psr::ApplyMagFood(registry, mag, 999));
    CHECK(registry.GetComponent<psr::MagComponent>(mag).pow_progress == 0);
}

TEST_CASE("ApplyMagFood increases IQ on every successful feed", "[MagFeeding]")
{
    psr::Registry registry;
    entt::entity mag = registry.CreateEntity();
    psr::MagComponent component;
    component.feed_response.push_back({/*item_prefab_id=*/7, /*pow=*/0, /*def=*/0, /*dex=*/0, /*mind=*/0});
    registry.Emplace<psr::MagComponent>(mag, component);

    REQUIRE(psr::ApplyMagFood(registry, mag, 7));
    CHECK(registry.GetComponent<psr::MagComponent>(mag).iq == 1);
}

TEST_CASE("ApplyMagFood evolves the mag once its evolution_tree condition is met, preserving runtime stats",
          "[MagFeeding][Evolution]")
{
    psr::Registry registry;
    MagSpeciesEntityLoader loader;
    registry.RegisterPrefabs(loader);

    entt::entity mag = registry.CreateEntity();
    registry.Emplace<psr::MagComponent>(mag, registry.GetPrefabComponent<psr::MagComponent>(kSpeciesAPrefabId));
    registry.Emplace<psr::RenderableComponent>(
        mag, registry.GetPrefabComponent<psr::RenderableComponent>(kSpeciesAPrefabId));
    registry.Emplace<psr::PrefabIdComponent>(mag, psr::PrefabIdComponent{kSpeciesAPrefabId});

    REQUIRE(psr::ApplyMagFood(registry, mag, 100)); // POW -> level 1, progress 1 (1 < 3, no evolution)
    CHECK(registry.GetComponent<psr::PrefabIdComponent>(mag).value == kSpeciesAPrefabId);

    REQUIRE(psr::ApplyMagFood(registry, mag, 100)); // POW -> level 2, progress 2 (still < 3)
    REQUIRE(psr::ApplyMagFood(registry, mag, 100)); // POW -> level 3, progress 3 (3 >= 3 -> evolves)

    const psr::MagComponent& evolved = registry.GetComponent<psr::MagComponent>(mag);
    CHECK(registry.GetComponent<psr::PrefabIdComponent>(mag).value == kSpeciesBPrefabId);
    CHECK(registry.GetComponent<psr::RenderableComponent>(mag).texture_id == 222);
    CHECK(evolved.feed_cooldown_turns == 80); // species B's own config, copied over
    CHECK(evolved.pow_level == 3);            // runtime stat progress preserved through the evolution
}

TEST_CASE("RegisterMagFeed starts the cooldown on the first feed of a charge cycle", "[MagFeeding]")
{
    psr::MagComponent mag;
    mag.feed_cooldown_turns = 50;
    mag.feed_charges = 3;

    psr::RegisterMagFeed(mag);
    CHECK(mag.feed_charges_used == 1);
    CHECK(mag.feed_cooldown_remaining == 50);
}

TEST_CASE("RegisterMagFeed allows further feeds within the same cycle without restarting the cooldown",
          "[MagFeeding]")
{
    psr::MagComponent mag;
    mag.feed_cooldown_turns = 50;
    mag.feed_charges = 3;

    psr::RegisterMagFeed(mag);
    mag.feed_cooldown_remaining = 40; // simulate a few elapsed turns

    psr::RegisterMagFeed(mag);
    CHECK(mag.feed_charges_used == 2);
    CHECK(mag.feed_cooldown_remaining == 40); // unchanged -- only the cycle's first feed (re)starts it

    psr::RegisterMagFeed(mag);
    CHECK(mag.feed_charges_used == 3);
    CHECK(mag.feed_cooldown_remaining == 40);
}

TEST_CASE("TickMagFeedCooldowns decrements every live mag's cooldown by one, never below zero, refilling "
          "feed_charges_used once a cooldown reaches zero",
          "[MagFeeding]")
{
    psr::Registry registry;

    entt::entity mag_a = registry.CreateEntity();
    psr::MagComponent component_a;
    component_a.feed_cooldown_remaining = 3;
    component_a.feed_charges_used = 2;
    registry.Emplace<psr::MagComponent>(mag_a, component_a);

    entt::entity mag_b = registry.CreateEntity();
    psr::MagComponent component_b;
    component_b.feed_cooldown_remaining = 1;
    component_b.feed_charges_used = 3;
    registry.Emplace<psr::MagComponent>(mag_b, component_b);

    entt::entity mag_c = registry.CreateEntity();
    psr::MagComponent component_c;
    component_c.feed_cooldown_remaining = 0;
    component_c.feed_charges_used = 0;
    registry.Emplace<psr::MagComponent>(mag_c, component_c);

    psr::TickMagFeedCooldowns(registry);

    CHECK(registry.GetComponent<psr::MagComponent>(mag_a).feed_cooldown_remaining == 2);
    CHECK(registry.GetComponent<psr::MagComponent>(mag_a).feed_charges_used == 2); // still recharging

    CHECK(registry.GetComponent<psr::MagComponent>(mag_b).feed_cooldown_remaining == 0);
    CHECK(registry.GetComponent<psr::MagComponent>(mag_b).feed_charges_used == 0); // just fully refilled

    CHECK(registry.GetComponent<psr::MagComponent>(mag_c).feed_cooldown_remaining == 0);
    CHECK(registry.GetComponent<psr::MagComponent>(mag_c).feed_charges_used == 0); // wasn't on cooldown, untouched
}

TEST_CASE("ApplyMagFood's self-referencing evolution rule is a correct no-op", "[MagFeeding][Evolution]")
{
    psr::Registry registry;
    SelfLoopEntityLoader loader;
    registry.RegisterPrefabs(loader);

    entt::entity mag = registry.CreateEntity();
    registry.Emplace<psr::MagComponent>(mag, registry.GetPrefabComponent<psr::MagComponent>(kSelfLoopPrefabId));
    registry.Emplace<psr::RenderableComponent>(mag);
    registry.Emplace<psr::PrefabIdComponent>(mag, psr::PrefabIdComponent{kSelfLoopPrefabId});

    REQUIRE(psr::ApplyMagFood(registry, mag, 7));

    const psr::MagComponent& result = registry.GetComponent<psr::MagComponent>(mag);
    CHECK(registry.GetComponent<psr::PrefabIdComponent>(mag).value == kSelfLoopPrefabId);
    CHECK(result.pow_level == 1);
    CHECK(result.pow_progress == 0);
    CHECK(result.feed_cooldown_turns == 50);
}
