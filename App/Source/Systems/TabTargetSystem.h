#pragma once

#include "Engine/Dungeon/RoomMap.h"
#include "Engine/Dungeon/RoomVisibilityTracker.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Math/Vec2.h"
#include "Engine/World/Grid.h"

namespace psr {

// Drives the player's persistent tab-lock target (TabTargetComponent) and
// the world marker shown on its tile. Owns a single marker entity, created
// once and repositioned via Grid::RemoveEntity/AddEntity as its target
// moves -- same idiom as TargetSelectionState's own cursor entity, chosen
// over StatusEffectWorldMarkers' destroy-and-recreate style because this
// marker needs cheap, potentially every-frame repositioning rather than
// reacting to a discrete per-turn event.
class TabTargetSystem
{
public:
    TabTargetSystem(Registry& registry, Grid& grid, const RoomMap& room_map, const RoomVisibilityTracker& visibility);

    // Tab: a fresh Manhattan-distance-sorted scan of every hostile
    // HealthComponent entity from player's tile (Hostility.h's IsHostile --
    // excludes the player itself; projectiles never carry HealthComponent,
    // so they're naturally excluded too) whose room is RoomVisibility::Visible
    // (matches FogOfWarRenderableLookup's own actor-hiding rule, so you can
    // never lock onto something the fog isn't drawing). Finds the current
    // target's index in that fresh list, if still present, and advances to
    // the next, wrapping after the last; otherwise (no target yet, or it
    // dropped out of the list) selects the nearest. Clears instead if the
    // list is empty. Repositions the marker to the new target's tile.
    void CycleTarget(Entity player);

    // Escape: clears TabTargetComponent::target and removes the marker.
    void ClearTarget(Entity player);

    // Call once per frame: if the current target died, was destroyed, or its
    // room fell out of RoomVisibility::Visible, clears it; otherwise
    // repositions the marker if the target's tile changed since last call.
    void Update(Entity player);

private:
    bool IsVisible(entt::entity target) const;
    void RepositionMarker(Vec2 tile);
    void RemoveMarker();

    Registry* m_registry;
    Grid* m_grid;
    const RoomMap* m_room_map;
    const RoomVisibilityTracker* m_visibility;
    entt::entity m_marker_entity = entt::null;
    Vec2 m_marker_tile;
};

} // namespace psr
