#pragma once

#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2.h"
#include "Engine/Math/Vec2f.h"

#include <string>
#include <vector>

namespace psr {

// One in-flight floating-text instance -- a short-lived, colored message
// drifting away from a world position (e.g. a damage number over an
// entity's head). Pure VFX with no gameplay interactions, so this is a
// plain owned value rather than an ECS entity/component: nothing else in
// the game ever needs to query "is there a floating text here", so there's
// no reason to spend a Registry entity (and its EventHandlerComponent) on
// one.
struct FloatingTextInstance
{
    Vec2 origin_tile;   // world tile the text started at
    Vec2f offset;       // accumulated drift since spawn, tile-fraction units
    Vec2f direction;    // caller-supplied unit vector, e.g. {0, -1} for "up" (+Y is down, matching Vec2/TileToPixel)
    float speed = 0.0f; // tiles per second
    std::string text;
    Color color;
    float duration = 0.0f;
    float elapsed = 0.0f;
};

// Generic engine-level system for short-lived colored text drifting away
// from a world position -- damage numbers are one consumer (see App's
// DamageTextSystem), not the only one. Theme-agnostic (no PSO vocabulary),
// so it lives in Core alongside Camera/TileVertexMath, the other pieces a
// caller needs to actually place this on screen.
class FloatingTextSystem
{
public:
    void Spawn(Vec2 origin_tile, std::string text, Color color, Vec2f direction, float speed, float duration);

    // Advances every instance's offset by direction * speed * delta_time and
    // removes any whose elapsed has reached duration. Call once per frame --
    // deliberately independent of any turn/state-machine gating (a caller
    // driving this from GameplayLayer::OnUpdate keeps it animating even
    // while a modal GameState is on top and TurnCoordinator::Step isn't
    // running).
    void Update(float delta_time);

    const std::vector<FloatingTextInstance>& Active() const { return m_instances; }

private:
    std::vector<FloatingTextInstance> m_instances;
};

} // namespace psr
