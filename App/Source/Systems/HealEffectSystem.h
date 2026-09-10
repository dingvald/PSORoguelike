#pragma once

#include "Engine/ECS/Entity.h"

namespace psr {

class VisualEffectSystem;
struct AfterHealEvent;

// Bridges AfterHealEvent onto VisualEffectSystem: spawns a short, wall-clock
// timed glow (see kHealEffectPrefabId/kHealEffectDuration in the .cpp) at the
// healed entity's own tile -- fixed prefab/duration, same "one constant
// effect, no per-source authoring" shape as MissFlashEffectSystem, not the
// per-technique data-driven shape AfterDamageEvent's hit_effect_prefab_id
// uses (no heal source authors its own effect yet). Not player-filtered,
// unlike MissFlashEffectSystem -- every healed entity should show its glow,
// same breadth as OnHitEffectSystem's own wiring. A zero actually-applied
// amount (AfterHealEvent::amount, e.g. an item used at full HP) spawns
// nothing.
class HealEffectSystem
{
public:
    explicit HealEffectSystem(VisualEffectSystem& visual_effects);

    // Wires one entity's EventHandlerComponent to this bridge. Call once per
    // actor as it's created -- see DamageTextSystem::Subscribe's own doc
    // comment for the full rationale, identical here.
    void Subscribe(Entity actor);

private:
    void OnHeal(Entity actor, AfterHealEvent& event);

    VisualEffectSystem* m_visual_effects;
};

} // namespace psr
