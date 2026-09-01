#include "Render/FogOfWarRenderableLookup.h"

#include "Components/AiComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RenderableComponent.h"
#include "Engine/ECS/Position.h"
#include "Render/RegistryRenderableLookup.h"

#include <catch2/catch_test_macros.hpp>

using namespace psr;

namespace {

// A mid-grey, non-black test color -- lets tests distinguish "darkened" from
// "unchanged" without depending on the exact darken factor.
constexpr Color kTestColor{200, 200, 200, 255};

entt::entity MakeRenderableEntity(Registry& registry, Vec2 tile)
{
    const entt::entity entity = registry.CreateEntity();
    registry.Emplace<Position>(entity, Position{tile});
    registry.Emplace<RenderableComponent>(entity, RenderableComponent{0, {}, {}, kTestColor, kTestColor, 0});
    return entity;
}

} // namespace

TEST_CASE("FogOfWarRenderableLookup hides geometry and actors in a never-visited room", "[FogOfWarRenderableLookup]")
{
    Registry registry;
    RegistryRenderableLookup inner(registry);
    RoomMap room_map(1, 2);
    room_map.SetRoom(Vec2{0, 0}, 0);
    room_map.SetRoom(Vec2{0, 1}, 1);
    RoomVisibilityTracker visibility(2);
    visibility.Update(0u); // room 1 is never visited

    FogOfWarRenderableLookup fog(registry, room_map, visibility, inner);

    const entt::entity geometry = MakeRenderableEntity(registry, Vec2{0, 1});
    const entt::entity actor = MakeRenderableEntity(registry, Vec2{0, 1});
    registry.Emplace<AiComponent>(actor);

    CHECK_FALSE(fog.GetRenderableTile(geometry).has_value());
    CHECK_FALSE(fog.GetRenderableTile(actor).has_value());
}

TEST_CASE("FogOfWarRenderableLookup dims geometry and hides actors in an explored, non-current room",
          "[FogOfWarRenderableLookup]")
{
    Registry registry;
    RegistryRenderableLookup inner(registry);
    RoomMap room_map(1, 2);
    room_map.SetRoom(Vec2{0, 0}, 0);
    room_map.SetRoom(Vec2{0, 1}, 1);
    RoomVisibilityTracker visibility(2);
    visibility.Update(0u); // visit room 0
    visibility.Update(1u); // move on to room 1 -- room 0 is now Explored

    FogOfWarRenderableLookup fog(registry, room_map, visibility, inner);

    const entt::entity geometry = MakeRenderableEntity(registry, Vec2{0, 0});
    const entt::entity enemy = MakeRenderableEntity(registry, Vec2{0, 0});
    registry.Emplace<AiComponent>(enemy);
    const entt::entity player = MakeRenderableEntity(registry, Vec2{0, 0});
    registry.Emplace<PlayerControlledComponent>(player);

    const std::optional<RenderableTile> dimmed = fog.GetRenderableTile(geometry);
    REQUIRE(dimmed.has_value());
    CHECK(dimmed->color_1.r < kTestColor.r);
    CHECK(dimmed->color_1.a == kTestColor.a); // alpha untouched, only RGB dims

    CHECK_FALSE(fog.GetRenderableTile(enemy).has_value());
    CHECK_FALSE(fog.GetRenderableTile(player).has_value());
}

TEST_CASE("FogOfWarRenderableLookup renders the current room unchanged", "[FogOfWarRenderableLookup]")
{
    Registry registry;
    RegistryRenderableLookup inner(registry);
    RoomMap room_map(1, 1);
    room_map.SetRoom(Vec2{0, 0}, 0);
    RoomVisibilityTracker visibility(1);
    visibility.Update(0u);

    FogOfWarRenderableLookup fog(registry, room_map, visibility, inner);

    const entt::entity enemy = MakeRenderableEntity(registry, Vec2{0, 0});
    registry.Emplace<AiComponent>(enemy);

    const std::optional<RenderableTile> tile = fog.GetRenderableTile(enemy);
    REQUIRE(tile.has_value());
    CHECK(tile->color_1.r == kTestColor.r);
}

TEST_CASE("FogOfWarRenderableLookup renders an adjacent, never-entered room unchanged like the current one",
          "[FogOfWarRenderableLookup]")
{
    Registry registry;
    RegistryRenderableLookup inner(registry);
    RoomMap room_map(1, 2);
    room_map.SetRoom(Vec2{0, 0}, 0);
    room_map.SetRoom(Vec2{0, 1}, 1);
    RoomVisibilityTracker visibility(2, {{1}, {0}}); // rooms 0 and 1 are adjacent
    visibility.Update(0u); // room 1 is adjacent, but never entered

    FogOfWarRenderableLookup fog(registry, room_map, visibility, inner);

    const entt::entity geometry = MakeRenderableEntity(registry, Vec2{0, 1});
    const entt::entity enemy = MakeRenderableEntity(registry, Vec2{0, 1});
    registry.Emplace<AiComponent>(enemy);

    const std::optional<RenderableTile> tile = fog.GetRenderableTile(geometry);
    REQUIRE(tile.has_value());
    CHECK(tile->color_1.r == kTestColor.r); // unchanged, not dimmed

    const std::optional<RenderableTile> enemy_tile = fog.GetRenderableTile(enemy);
    REQUIRE(enemy_tile.has_value()); // actors stay visible in an adjacent room, unlike Explored
}

TEST_CASE("FogOfWarRenderableLookup passes through entities with no Position regardless of room state",
          "[FogOfWarRenderableLookup]")
{
    Registry registry;
    RegistryRenderableLookup inner(registry);
    RoomMap room_map(1, 1); // (0,0) deliberately left untagged
    RoomVisibilityTracker visibility(1);
    // Never call Update -- every room stays Hidden.

    FogOfWarRenderableLookup fog(registry, room_map, visibility, inner);

    // Mirrors a status-effect marker/target cursor: added to the grid but
    // never given a Position.
    const entt::entity marker = registry.CreateEntity();
    registry.Emplace<RenderableComponent>(marker, RenderableComponent{0, {}, {}, kTestColor, kTestColor, 0});

    const std::optional<RenderableTile> tile = fog.GetRenderableTile(marker);
    REQUIRE(tile.has_value());
    CHECK(tile->color_1.r == kTestColor.r);
}
