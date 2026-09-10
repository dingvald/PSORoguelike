#pragma once

namespace psr {

// Runtime-only countdown, in in-game turns (not wall-clock time) -- ticked
// down once per LifetimeSystem::Tick() call, never per frame. Reaching 0
// destroys the entity on that same Tick(). Stamped by whoever spawns a
// turn-lifetime-bound entity (e.g. GameplayLayer's enemy-spawn VFX), never
// authored/cloned via a prefab, deliberately not passed through
// ComponentSchemaRegistrar (mirrors SpawnWaveComponent).
struct LifetimeComponent
{
    int remaining_turns = 1;
};

} // namespace psr
