#include "Systems/EnemyAiSystem.h"

#include "Actions/AttackAction.h"
#include "Actions/MoveAction.h"
#include "Actions/TechniqueAction.h"
#include "Actions/WaitAction.h"
#include "Combat/Technique.h"
#include "Combat/TechniqueLibrary.h"
#include "Components/AiComponent.h"
#include "Components/BlocksMovementComponent.h"
#include "Components/KnownTechniquesComponent.h"
#include "Components/PackFollowerComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RaceComponent.h"
#include "Components/RangedTechComponent.h"
#include "Components/SpawnedByComponent.h"
#include "Components/SpawnerAiComponent.h"
#include "Components/StatsComponent.h"
#include "Components/TPComponent.h"
#include "Engine/ECS/ComponentMeta.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/IEntityLoader.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"
#include "Items/AffixLibrary.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/core/hashed_string.hpp>

#include <filesystem>
#include <random>
#include <unordered_map>
#include <vector>

TEST_CASE("EnemyAiSystem steps toward a distant hostile target", "[EnemyAiSystem]")
{
    psr::Registry registry;
    psr::Grid grid(5, 1);
    psr::AffixLibrary affixes;
    std::mt19937 rng{0};
    psr::TechniqueLibrary techniques;
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, rng};

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{0, 0}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::ChaseAndAttack, 8});
    grid.AddEntity(psr::Vec2{0, 0}, actor);

    const entt::entity target = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(target);
    registry.Emplace<psr::Position>(target, psr::Position{psr::Vec2{3, 0}});
    grid.AddEntity(psr::Vec2{3, 0}, target);

    psr::IAction* action = ai.Decide(psr::Entity(registry, actor));
    REQUIRE(dynamic_cast<psr::MoveAction*>(action) != nullptr);

    const psr::ActionResult result = action->Perform(psr::Entity(registry, actor));

    CHECK(result.cost == psr::MoveAction::kMoveCost);
    CHECK(registry.GetComponent<psr::Position>(actor).tile == psr::Vec2{1, 0});
}

TEST_CASE("EnemyAiSystem's step into an adjacent hostile falls back to an attack", "[EnemyAiSystem]")
{
    psr::Registry registry;
    psr::Grid grid(2, 1);
    psr::AffixLibrary affixes;
    std::mt19937 rng{0};
    psr::TechniqueLibrary techniques;
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, rng};

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{0, 0}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::ChaseAndAttack, 8});
    grid.AddEntity(psr::Vec2{0, 0}, actor);

    const entt::entity target = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(target);
    registry.Emplace<psr::Position>(target, psr::Position{psr::Vec2{1, 0}});
    registry.Emplace<psr::HealthComponent>(target, psr::HealthComponent{10, 10});
    registry.Emplace<psr::BlocksMovementComponent>(target);
    grid.AddEntity(psr::Vec2{1, 0}, target);

    psr::IAction* action = ai.Decide(psr::Entity(registry, actor));
    REQUIRE(dynamic_cast<psr::MoveAction*>(action) != nullptr);

    const psr::ActionResult result = action->Perform(psr::Entity(registry, actor));

    // Zero-cost move-that-became-a-bump: the fallback is the actual attack.
    CHECK(result.cost == 0);
    REQUIRE(result.fallback != nullptr);
    CHECK(dynamic_cast<psr::AttackAction*>(result.fallback.get()) != nullptr);
    // Never actually stepped onto the target's tile.
    CHECK(registry.GetComponent<psr::Position>(actor).tile == psr::Vec2{0, 0});
}

TEST_CASE("EnemyAiSystem waits when no hostile target is within detection_range", "[EnemyAiSystem]")
{
    psr::Registry registry;
    psr::Grid grid(10, 1);
    psr::AffixLibrary affixes;
    std::mt19937 rng{0};
    psr::TechniqueLibrary techniques;
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, rng};

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{0, 0}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::ChaseAndAttack, 2});
    grid.AddEntity(psr::Vec2{0, 0}, actor);

    const entt::entity target = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(target);
    registry.Emplace<psr::Position>(target, psr::Position{psr::Vec2{9, 0}}); // well past detection_range
    grid.AddEntity(psr::Vec2{9, 0}, target);

    psr::IAction* action = ai.Decide(psr::Entity(registry, actor));
    REQUIRE(dynamic_cast<psr::WaitAction*>(action) != nullptr);

    action->Perform(psr::Entity(registry, actor));
    CHECK(registry.GetComponent<psr::Position>(actor).tile == psr::Vec2{0, 0});
}

TEST_CASE("EnemyAiSystem waits (does not loop) when both candidate directions are walled off", "[EnemyAiSystem]")
{
    psr::Registry registry;
    psr::Grid grid(3, 3);
    psr::AffixLibrary affixes;
    std::mt19937 rng{0};
    psr::TechniqueLibrary techniques;
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, rng};

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{1, 1}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::ChaseAndAttack, 8});
    grid.AddEntity(psr::Vec2{1, 1}, actor);

    // Target diagonally beyond (2,2): both single-step candidates toward it
    // from (1,1) are (2,1) and (1,2) -- wall both off (BlocksMovementComponent,
    // no HealthComponent, matching wall.json's own shape).
    const entt::entity target = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(target);
    registry.Emplace<psr::Position>(target, psr::Position{psr::Vec2{2, 2}});
    grid.AddEntity(psr::Vec2{2, 2}, target);

    const entt::entity wall_a = registry.CreateEntity();
    registry.Emplace<psr::BlocksMovementComponent>(wall_a);
    grid.AddEntity(psr::Vec2{2, 1}, wall_a);

    const entt::entity wall_b = registry.CreateEntity();
    registry.Emplace<psr::BlocksMovementComponent>(wall_b);
    grid.AddEntity(psr::Vec2{1, 2}, wall_b);

    psr::IAction* action = ai.Decide(psr::Entity(registry, actor));
    REQUIRE(dynamic_cast<psr::WaitAction*>(action) != nullptr);

    action->Perform(psr::Entity(registry, actor));
    CHECK(registry.GetComponent<psr::Position>(actor).tile == psr::Vec2{1, 1});
}

TEST_CASE("EnemyAiSystem's FleeWhenHit approaches like ChaseAndAttack while undamaged", "[EnemyAiSystem]")
{
    psr::Registry registry;
    psr::Grid grid(5, 1);
    psr::AffixLibrary affixes;
    psr::TechniqueLibrary techniques;
    std::mt19937 rng{0};
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, rng};

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{0, 0}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::FleeWhenHit, 8});
    registry.Emplace<psr::HealthComponent>(actor, psr::HealthComponent{20, 20}); // undamaged
    grid.AddEntity(psr::Vec2{0, 0}, actor);

    const entt::entity target = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(target);
    registry.Emplace<psr::Position>(target, psr::Position{psr::Vec2{3, 0}});
    grid.AddEntity(psr::Vec2{3, 0}, target);

    psr::IAction* action = ai.Decide(psr::Entity(registry, actor));
    REQUIRE(dynamic_cast<psr::MoveAction*>(action) != nullptr);

    action->Perform(psr::Entity(registry, actor));
    CHECK(registry.GetComponent<psr::Position>(actor).tile == psr::Vec2{1, 0}); // stepped toward
}

TEST_CASE("EnemyAiSystem's FleeWhenHit steps away once it has taken any damage", "[EnemyAiSystem]")
{
    psr::Registry registry;
    psr::Grid grid(5, 1);
    psr::AffixLibrary affixes;
    psr::TechniqueLibrary techniques;
    std::mt19937 rng{0};
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, rng};

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{2, 0}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::FleeWhenHit, 8});
    registry.Emplace<psr::HealthComponent>(actor, psr::HealthComponent{15, 20}); // damaged
    grid.AddEntity(psr::Vec2{2, 0}, actor);

    const entt::entity target = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(target);
    registry.Emplace<psr::Position>(target, psr::Position{psr::Vec2{0, 0}});
    grid.AddEntity(psr::Vec2{0, 0}, target);

    psr::IAction* action = ai.Decide(psr::Entity(registry, actor));
    REQUIRE(dynamic_cast<psr::MoveAction*>(action) != nullptr);

    action->Perform(psr::Entity(registry, actor));
    CHECK(registry.GetComponent<psr::Position>(actor).tile == psr::Vec2{3, 0}); // stepped away from the target
}

namespace {
// Throwaway test fixture prefab -- not real content, per CLAUDE.md's
// test-fixture carve-out. Mirrors LootDropSystemTests.cpp's own
// ItemMarker/TestEntityLoader pattern, needed so Registry::CreateEntity(id)
// has something real to instantiate for a spawned Mothmant stand-in.
struct SpawnedMarker
{
    static void Register(entt::meta_ctx& ctx)
    {
        using namespace entt::literals;
        entt::meta_factory<SpawnedMarker>(ctx).func<&psr::CloneComponent<SpawnedMarker>>("clone"_hs);
    }
};

constexpr std::uint32_t kSpawnedPrefab = 1;

class SpawnedPrefabLoader : public psr::IEntityLoader
{
public:
    bool Load(std::filesystem::path /*path*/) override { return true; }

    void Populate(entt::registry& prefab_registry,
                  std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
    {
        const entt::entity spawned = prefab_registry.create();
        prefab_registry.emplace<SpawnedMarker>(spawned);
        out_prefab_ids.emplace(kSpawnedPrefab, spawned);
    }
};
} // namespace

TEST_CASE("EnemyAiSystem's StationarySpawner spawns into an adjacent tile once its cooldown reaches zero",
          "[EnemyAiSystem]")
{
    psr::Registry registry;
    SpawnedMarker::Register(registry.GetMetaContext());
    SpawnedPrefabLoader loader;
    registry.RegisterPrefabs(loader);

    psr::Grid grid(3, 3);
    psr::AffixLibrary affixes;
    psr::TechniqueLibrary techniques;
    std::mt19937 rng{0};
    int spawned_count = 0;
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, rng, [&](entt::entity) { ++spawned_count; }};

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{1, 1}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::StationarySpawner, 8});
    registry.Emplace<psr::SpawnerAiComponent>(actor, psr::SpawnerAiComponent{kSpawnedPrefab, /*cooldown_turns=*/3,
                                                                             /*max_alive=*/2});
    grid.AddEntity(psr::Vec2{1, 1}, actor);

    psr::IAction* action = ai.Decide(psr::Entity(registry, actor));
    REQUIRE(dynamic_cast<psr::WaitAction*>(action) != nullptr); // never moves/attacks itself
    CHECK(spawned_count == 1);
    CHECK(registry.GetComponent<psr::SpawnerAiComponent>(actor).cooldown_remaining == 3);
}

TEST_CASE("EnemyAiSystem's StationarySpawner does not spawn past max_alive", "[EnemyAiSystem]")
{
    psr::Registry registry;
    SpawnedMarker::Register(registry.GetMetaContext());
    SpawnedPrefabLoader loader;
    registry.RegisterPrefabs(loader);

    psr::Grid grid(3, 3);
    psr::AffixLibrary affixes;
    psr::TechniqueLibrary techniques;
    std::mt19937 rng{0};
    int spawned_count = 0;
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, rng, [&](entt::entity) { ++spawned_count; }};

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{1, 1}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::StationarySpawner, 8});
    registry.Emplace<psr::SpawnerAiComponent>(actor, psr::SpawnerAiComponent{kSpawnedPrefab, /*cooldown_turns=*/1,
                                                                             /*max_alive=*/1});
    grid.AddEntity(psr::Vec2{1, 1}, actor);

    ai.Decide(psr::Entity(registry, actor)); // first spawn succeeds
    CHECK(spawned_count == 1);

    ai.Decide(psr::Entity(registry, actor)); // cooldown_remaining now 1, decrements, no spawn yet
    CHECK(spawned_count == 1);

    ai.Decide(psr::Entity(registry, actor)); // cooldown hits 0, but max_alive already reached
    CHECK(spawned_count == 1);
}

TEST_CASE("EnemyAiSystem's PackFollower applies a one-time stat penalty once no pack leader is nearby",
          "[EnemyAiSystem]")
{
    psr::Registry registry;
    psr::Grid grid(5, 1);
    psr::AffixLibrary affixes;
    psr::TechniqueLibrary techniques;
    std::mt19937 rng{0};
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, rng};

    constexpr std::uint32_t kLeaderRace = 42;

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{0, 0}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::PackFollower, 8});
    registry.Emplace<psr::PackFollowerComponent>(actor, psr::PackFollowerComponent{kLeaderRace});
    registry.Emplace<psr::StatsComponent>(actor, psr::StatsComponent{100, 0, 0, 50, 0, 0});
    grid.AddEntity(psr::Vec2{0, 0}, actor);

    ai.Decide(psr::Entity(registry, actor));

    CHECK(registry.GetComponent<psr::StatsComponent>(actor).atp == 70);
    CHECK(registry.GetComponent<psr::StatsComponent>(actor).dfp == 35);
    CHECK(registry.GetComponent<psr::PackFollowerComponent>(actor).has_panicked);

    ai.Decide(psr::Entity(registry, actor)); // must not reapply
    CHECK(registry.GetComponent<psr::StatsComponent>(actor).atp == 70);
}

TEST_CASE("EnemyAiSystem's PackFollower does not panic while a living pack leader is nearby", "[EnemyAiSystem]")
{
    psr::Registry registry;
    psr::Grid grid(5, 1);
    psr::AffixLibrary affixes;
    psr::TechniqueLibrary techniques;
    std::mt19937 rng{0};
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, rng};

    constexpr std::uint32_t kLeaderRace = 42;

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{0, 0}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::PackFollower, 8});
    registry.Emplace<psr::PackFollowerComponent>(actor, psr::PackFollowerComponent{kLeaderRace});
    registry.Emplace<psr::StatsComponent>(actor, psr::StatsComponent{100, 0, 0, 50, 0, 0});
    grid.AddEntity(psr::Vec2{0, 0}, actor);

    const entt::entity leader = registry.CreateEntity();
    registry.Emplace<psr::Position>(leader, psr::Position{psr::Vec2{1, 0}});
    registry.Emplace<psr::RaceComponent>(leader, psr::RaceComponent{kLeaderRace});
    grid.AddEntity(psr::Vec2{1, 0}, leader);

    ai.Decide(psr::Entity(registry, actor));

    CHECK(registry.GetComponent<psr::StatsComponent>(actor).atp == 100);
    CHECK_FALSE(registry.GetComponent<psr::PackFollowerComponent>(actor).has_panicked);
}

TEST_CASE("EnemyAiSystem's RangedTechAtDistance casts when aligned and in range", "[EnemyAiSystem]")
{
    psr::Registry registry;
    psr::Grid grid(6, 6);
    psr::AffixLibrary affixes;
    constexpr std::uint32_t kTechniqueId = 7;
    psr::Technique technique;
    technique.id = kTechniqueId;
    technique.tp_cost = 4;
    psr::TechniqueLibrary techniques{std::vector<psr::Technique>{technique}};
    std::mt19937 rng{0};
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, rng};

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{0, 0}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::RangedTechAtDistance, 8});
    registry.Emplace<psr::RangedTechComponent>(actor, psr::RangedTechComponent{kTechniqueId, 5});
    registry.Emplace<psr::TPComponent>(actor, psr::TPComponent{10, 10});
    registry.Emplace<psr::KnownTechniquesComponent>(
        actor, psr::KnownTechniquesComponent{{psr::KnownTechniqueEntry{kTechniqueId, 1}}});
    grid.AddEntity(psr::Vec2{0, 0}, actor);

    const entt::entity target = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(target);
    registry.Emplace<psr::Position>(target, psr::Position{psr::Vec2{3, 0}}); // aligned row, distance 3, in range
    grid.AddEntity(psr::Vec2{3, 0}, target);

    psr::IAction* action = ai.Decide(psr::Entity(registry, actor));
    REQUIRE(dynamic_cast<psr::TechniqueAction*>(action) != nullptr);
    REQUIRE(registry.HasComponent<psr::SelectedTargetComponent>(actor));
    CHECK(registry.GetComponent<psr::SelectedTargetComponent>(actor).tile == psr::Vec2{3, 0});
}

TEST_CASE("EnemyAiSystem's RangedTechAtDistance melees when adjacent instead of casting", "[EnemyAiSystem]")
{
    psr::Registry registry;
    psr::Grid grid(3, 1);
    psr::AffixLibrary affixes;
    constexpr std::uint32_t kTechniqueId = 7;
    psr::Technique technique;
    technique.id = kTechniqueId;
    psr::TechniqueLibrary techniques{std::vector<psr::Technique>{technique}};
    std::mt19937 rng{0};
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, rng};

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{0, 0}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::RangedTechAtDistance, 8});
    registry.Emplace<psr::RangedTechComponent>(actor, psr::RangedTechComponent{kTechniqueId, 5});
    registry.Emplace<psr::TPComponent>(actor, psr::TPComponent{10, 10});
    registry.Emplace<psr::KnownTechniquesComponent>(
        actor, psr::KnownTechniquesComponent{{psr::KnownTechniqueEntry{kTechniqueId, 1}}});
    grid.AddEntity(psr::Vec2{0, 0}, actor);

    const entt::entity target = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(target);
    registry.Emplace<psr::Position>(target, psr::Position{psr::Vec2{1, 0}}); // adjacent
    registry.Emplace<psr::HealthComponent>(target, psr::HealthComponent{10, 10});
    registry.Emplace<psr::BlocksMovementComponent>(target);
    grid.AddEntity(psr::Vec2{1, 0}, target);

    psr::IAction* action = ai.Decide(psr::Entity(registry, actor));
    REQUIRE(dynamic_cast<psr::MoveAction*>(action) != nullptr);
}

TEST_CASE("EnemyAiSystem's RangedTechAtDistance closes distance when not aligned with the target", "[EnemyAiSystem]")
{
    psr::Registry registry;
    psr::Grid grid(6, 6);
    psr::AffixLibrary affixes;
    constexpr std::uint32_t kTechniqueId = 7;
    psr::Technique technique;
    technique.id = kTechniqueId;
    psr::TechniqueLibrary techniques{std::vector<psr::Technique>{technique}};
    std::mt19937 rng{0};
    psr::EnemyAiSystem ai{grid, registry, affixes, techniques, rng};

    const entt::entity actor = registry.CreateEntity();
    registry.Emplace<psr::Position>(actor, psr::Position{psr::Vec2{0, 0}});
    registry.Emplace<psr::AiComponent>(actor, psr::AiComponent{psr::AiBehavior::RangedTechAtDistance, 8});
    registry.Emplace<psr::RangedTechComponent>(actor, psr::RangedTechComponent{kTechniqueId, 5});
    registry.Emplace<psr::TPComponent>(actor, psr::TPComponent{10, 10});
    registry.Emplace<psr::KnownTechniquesComponent>(
        actor, psr::KnownTechniquesComponent{{psr::KnownTechniqueEntry{kTechniqueId, 1}}});
    grid.AddEntity(psr::Vec2{0, 0}, actor);

    const entt::entity target = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(target);
    registry.Emplace<psr::Position>(target, psr::Position{psr::Vec2{3, 2}}); // diagonal, not aligned
    grid.AddEntity(psr::Vec2{3, 2}, target);

    psr::IAction* action = ai.Decide(psr::Entity(registry, actor));
    REQUIRE(dynamic_cast<psr::TechniqueAction*>(action) == nullptr);
    REQUIRE(dynamic_cast<psr::MoveAction*>(action) != nullptr);
}
