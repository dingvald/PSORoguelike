#pragma once

#include "Engine/ECS/Entity.h"
#include "Engine/Math/Vec2.h"

#include <entt/entt.hpp>

#include <vector>

namespace psr {

class Registry;
class Grid;
class StatusEffectLibrary;
struct AfterStatusEffectsChangedEvent;

// Draws "status icon ... over affected entities" (ROADMAP.md's M7.3 UI
// bullet) by spawning/despawning small tinted marker entities in the live
// Grid -- reuses the exact data-driven-ECS-entity precedent M7.2 already set
// for the target-select cursor (App/Assets/Data/Entities/ui/
// status_effect_marker.json), rather than touching TileRenderer/renderer
// internals. One marker per distinct active StatusEffectType, repositioned
// to the tracked entity's current tile every time
// AfterStatusEffectsChangedEvent fires -- which, per
// StatusEffectApplication.h's TickStatusEffects, fires once per turn for as
// long as any stack stays active, so a marker never lags behind its entity's
// own movement by more than that entity's own last turn (no separate
// AfterMoveEvent subscription needed).
//
// Scoped to a single tracked entity -- only the player is wired today,
// mirroring CombatLogBridge's identical single-caller scope ("no enemies
// spawn yet").
class StatusEffectWorldMarkers
{
public:
    StatusEffectWorldMarkers(Registry& registry, Grid& grid, const StatusEffectLibrary& status_effects);
    ~StatusEffectWorldMarkers();

    // Bound handlers capture this instance's address -- neither copying nor
    // moving would keep them valid, same rationale as CombatLogBridge's
    // identical restriction.
    StatusEffectWorldMarkers(const StatusEffectWorldMarkers&) = delete;
    StatusEffectWorldMarkers& operator=(const StatusEffectWorldMarkers&) = delete;
    StatusEffectWorldMarkers(StatusEffectWorldMarkers&&) = delete;
    StatusEffectWorldMarkers& operator=(StatusEffectWorldMarkers&&) = delete;

    // Wires tracked's EventHandlerComponent to this instance. Call once, for
    // the one entity this instance tracks (the player, today).
    void Subscribe(Entity tracked);

private:
    void OnStatusEffectsChanged(Entity actor, AfterStatusEffectsChangedEvent& event);
    void ClearMarkers();

    Registry* m_registry;
    Grid* m_grid;
    const StatusEffectLibrary* m_status_effects;

    entt::entity m_tracked = entt::null;
    std::vector<entt::entity> m_markers;
    Vec2 m_marker_tile;
};

} // namespace psr
