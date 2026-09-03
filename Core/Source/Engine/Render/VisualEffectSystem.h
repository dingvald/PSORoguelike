#pragma once

#include "Engine/Math/Easing.h"
#include "Engine/Math/Vec2.h"

#include <entt/entt.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace psr {

class Registry;
class Grid;

// One in-flight visual-effect entity: placed on the Grid at tile, eased from
// start_alpha to end_alpha over duration, then removed and destroyed. tile is
// snapshotted at Spawn time and never re-read from the entity's own Position
// -- effects are short/cosmetic, so a target moving away mid-effect is not a
// case this needs to handle.
struct VisualEffectInstance
{
    entt::entity entity = entt::null;
    Vec2 tile;
    float duration = 0.0f;
    float elapsed = 0.0f;
    EasingCurve easing = EasingCurve::Linear;
    std::uint8_t start_alpha = 255;
    std::uint8_t end_alpha = 0;
};

// Generic engine-level system for short-lived, prefab-authored visual effect
// entities that fade out over time -- a player-miss flash is one consumer
// (see App's MissFlashEffectSystem), not the only one. Theme-agnostic (no PSO
// vocabulary), so it lives in Core alongside FloatingTextSystem, the sibling
// system for non-ECS drifting text.
//
// Unlike FloatingTextSystem, an instance here is a real Grid-placed ECS
// entity (so it renders through the normal tile pipeline), which means
// fading it means writing to an App-only RenderableComponent -- Core can't
// name that type, so alpha_sink is how a caller plugs in the write without
// VisualEffectSystem knowing what it's writing to (same Core/App boundary
// IRenderableLookup already enforces, just via an injected callback since
// there's exactly one caller wiring exactly one behavior).
class VisualEffectSystem
{
public:
    VisualEffectSystem(Registry& registry, Grid& grid, std::function<void(entt::entity, std::uint8_t)> alpha_sink);

    // Clones prefab_id, places it at tile, and tracks it for duration seconds,
    // easing its alpha from start_alpha to end_alpha via easing. Returns
    // entt::null if prefab_id is unregistered -- checked here via HasPrefab
    // rather than left to Registry::CreateEntity(prefab_id), whose own
    // unknown-id check is only an assert (a no-op, so UB, in release builds).
    entt::entity Spawn(std::uint32_t prefab_id, Vec2 tile, float duration, EasingCurve easing,
                       std::uint8_t start_alpha = 255, std::uint8_t end_alpha = 0);

    // Advances every instance's elapsed time, pushes the eased alpha through
    // alpha_sink, and removes+destroys any instance whose elapsed has reached
    // duration. Call once per frame -- deliberately independent of any turn/
    // state-machine gating, same reasoning as FloatingTextSystem::Update.
    void Update(float delta_time);

private:
    Registry* m_registry;
    Grid* m_grid;
    std::function<void(entt::entity, std::uint8_t)> m_alpha_sink;
    std::vector<VisualEffectInstance> m_instances;
};

} // namespace psr
