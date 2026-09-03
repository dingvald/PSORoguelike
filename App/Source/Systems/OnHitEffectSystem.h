#pragma once

#include "Engine/ECS/Entity.h"

namespace psr {

class VisualEffectSystem;
struct AfterDamageEvent;

// Bridges AfterDamageEvent onto VisualEffectSystem: spawns whatever prefab
// the landed hit carried (AfterDamageEvent::hit_effect_prefab_id, threaded
// through from IncomingDamageEvent -- see OnHitEffectComponent.h and
// Technique::hit_effect_prefab_id for the two places that field comes
// from) at the target's tile. Unlike MissFlashEffectSystem, not filtered to
// the player -- every hit from every source should show its effect, same
// breadth as DamageTextSystem/CombatLogBridge's own wiring. A hit_effect_id
// of 0 (the default -- no OnHitEffectComponent authored, or a technique
// with no hit_effect_prefab_id set) spawns nothing.
class OnHitEffectSystem
{
public:
    explicit OnHitEffectSystem(VisualEffectSystem& visual_effects);

    // Wires one entity's EventHandlerComponent to this bridge. Call once per
    // actor as it's created -- see DamageTextSystem::Subscribe's own doc
    // comment for the full rationale, identical here.
    void Subscribe(Entity actor);

private:
    void OnHit(Entity actor, AfterDamageEvent& event);

    VisualEffectSystem* m_visual_effects;
};

} // namespace psr
