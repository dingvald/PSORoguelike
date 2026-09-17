#include "Systems/TabTargetSystem.h"

#include "Combat/Faction.h"
#include "Components/AiComponent.h"
#include "Components/FactionComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/TabTargetComponent.h"
#include "Engine/Dungeon/RoomMap.h"
#include "Engine/Dungeon/RoomVisibilityTracker.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/IEntityLoader.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/core/hashed_string.hpp>

#include <filesystem>
#include <unordered_map>

namespace {
    psr::RoomMap MakeSingleRoomMap(int width, int height)
    {
        psr::RoomMap room_map(width, height);
        for (int x = 0; x < width; ++x)
            for (int y = 0; y < height; ++y)
                room_map.SetRoom(psr::Vec2{x, y}, 0);
        return room_map;
    }

    const std::uint32_t kMarkerPrefabId = entt::hashed_string::value("ui.tab_target_marker");

    // A minimal stand-in for the real ui/tab_target_marker.json prefab -- just
    // enough for TabTargetSystem::RepositionMarker to have something to spawn
    // (see TargetSelectionStateTests.cpp's CursorEntityLoader for the same idiom).
    class MarkerEntityLoader : public psr::IEntityLoader
    {
    public:
        bool Load(std::filesystem::path /*path*/) override { return true; }

        void Populate(entt::registry& prefab_registry,
                      std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
        {
            out_prefab_ids.emplace(kMarkerPrefabId, prefab_registry.create());
        }
    };
} // namespace

TEST_CASE("TabTargetSystem retargets to the nearest remaining hostile once the current target is destroyed",
          "[TabTargetSystem]")
{
    psr::Registry registry;
    MarkerEntityLoader loader;
    registry.RegisterPrefabs(loader);
    psr::Grid grid(10, 1);
    psr::RoomMap room_map = MakeSingleRoomMap(10, 1);
    psr::RoomVisibilityTracker visibility(1);
    visibility.Update(0);

    psr::TabTargetSystem tab_target_system(registry, grid, room_map, visibility);

    const entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::Position>(player, psr::Position{psr::Vec2{0, 0}});
    grid.AddEntity(psr::Vec2{0, 0}, player);
    psr::Entity player_entity(registry, player);

    const entt::entity near = registry.CreateEntity();
    registry.Emplace<psr::AiComponent>(near);
    registry.Emplace<psr::HealthComponent>(near, psr::HealthComponent{10, 10});
    registry.Emplace<psr::Position>(near, psr::Position{psr::Vec2{2, 0}});
    grid.AddEntity(psr::Vec2{2, 0}, near);

    const entt::entity far = registry.CreateEntity();
    registry.Emplace<psr::AiComponent>(far);
    registry.Emplace<psr::HealthComponent>(far, psr::HealthComponent{10, 10});
    registry.Emplace<psr::Position>(far, psr::Position{psr::Vec2{5, 0}});
    grid.AddEntity(psr::Vec2{5, 0}, far);

    tab_target_system.CycleTarget(player_entity); // locks onto the nearest -- `near`
    REQUIRE(registry.GetComponent<psr::TabTargetComponent>(player).target == near);

    // Simulate DeathSystem destroying `near` mid-turn, before the next frame's Update().
    grid.RemoveEntity(psr::Vec2{2, 0}, near);
    registry.DestroyEntity(near);

    tab_target_system.Update(player_entity);

    CHECK(registry.GetComponent<psr::TabTargetComponent>(player).target == far);
}

TEST_CASE("TabTargetSystem clears the target once the last hostile is destroyed", "[TabTargetSystem]")
{
    psr::Registry registry;
    MarkerEntityLoader loader;
    registry.RegisterPrefabs(loader);
    psr::Grid grid(10, 1);
    psr::RoomMap room_map = MakeSingleRoomMap(10, 1);
    psr::RoomVisibilityTracker visibility(1);
    visibility.Update(0);

    psr::TabTargetSystem tab_target_system(registry, grid, room_map, visibility);

    const entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::Position>(player, psr::Position{psr::Vec2{0, 0}});
    grid.AddEntity(psr::Vec2{0, 0}, player);
    psr::Entity player_entity(registry, player);

    const entt::entity only = registry.CreateEntity();
    registry.Emplace<psr::AiComponent>(only);
    registry.Emplace<psr::HealthComponent>(only, psr::HealthComponent{10, 10});
    registry.Emplace<psr::Position>(only, psr::Position{psr::Vec2{2, 0}});
    grid.AddEntity(psr::Vec2{2, 0}, only);

    tab_target_system.CycleTarget(player_entity);
    REQUIRE(registry.GetComponent<psr::TabTargetComponent>(player).target == only);

    grid.RemoveEntity(psr::Vec2{2, 0}, only);
    registry.DestroyEntity(only);

    tab_target_system.Update(player_entity);

    CHECK((registry.GetComponent<psr::TabTargetComponent>(player).target == entt::null));
}

TEST_CASE("TabTargetSystem prefers a real (AiComponent) enemy over a hostile prop like a box, until no real enemy "
          "remains",
          "[TabTargetSystem]")
{
    psr::Registry registry;
    MarkerEntityLoader loader;
    registry.RegisterPrefabs(loader);
    psr::Grid grid(10, 1);
    psr::RoomMap room_map = MakeSingleRoomMap(10, 1);
    psr::RoomVisibilityTracker visibility(1);
    visibility.Update(0);

    psr::TabTargetSystem tab_target_system(registry, grid, room_map, visibility);

    const entt::entity player = registry.CreateEntity();
    registry.Emplace<psr::PlayerControlledComponent>(player);
    registry.Emplace<psr::Position>(player, psr::Position{psr::Vec2{0, 0}});
    grid.AddEntity(psr::Vec2{0, 0}, player);
    psr::Entity player_entity(registry, player);

    // A box: hostile faction (so bump-attacks work) but no AiComponent, and
    // closer to the player than the real enemy -- if faction alone drove
    // selection this would win on distance.
    const entt::entity box = registry.CreateEntity();
    registry.Emplace<psr::FactionComponent>(box, psr::FactionComponent{psr::Faction::Enemy});
    registry.Emplace<psr::HealthComponent>(box, psr::HealthComponent{15, 15});
    registry.Emplace<psr::Position>(box, psr::Position{psr::Vec2{1, 0}});
    grid.AddEntity(psr::Vec2{1, 0}, box);

    const entt::entity enemy = registry.CreateEntity();
    registry.Emplace<psr::AiComponent>(enemy);
    registry.Emplace<psr::HealthComponent>(enemy, psr::HealthComponent{10, 10});
    registry.Emplace<psr::Position>(enemy, psr::Position{psr::Vec2{4, 0}});
    grid.AddEntity(psr::Vec2{4, 0}, enemy);

    tab_target_system.CycleTarget(player_entity);
    CHECK(registry.GetComponent<psr::TabTargetComponent>(player).target == enemy);

    tab_target_system.CycleTarget(player_entity); // wraps: still only `enemy` is eligible
    CHECK(registry.GetComponent<psr::TabTargetComponent>(player).target == enemy);

    grid.RemoveEntity(psr::Vec2{4, 0}, enemy);
    registry.DestroyEntity(enemy);

    tab_target_system.Update(player_entity); // real enemy gone -- falls back to the box
    CHECK(registry.GetComponent<psr::TabTargetComponent>(player).target == box);
}
