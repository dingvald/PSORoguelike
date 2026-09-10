#pragma once

#include "Engine/ECS/Entity.h"

namespace psr {

class FloatingTextSystem;
struct AfterHealEvent;

// Bridges AfterHealEvent onto FloatingTextSystem: a green "+N" floating
// number drifts up from the healed entity's tile for every actually-applied
// heal -- DamageTextSystem's own white damage number, mirrored for the heal
// direction. A zero actually-applied amount (AfterHealEvent::amount, e.g. an
// item used at full HP) spawns nothing.
class HealTextSystem
{
public:
    explicit HealTextSystem(FloatingTextSystem& floating_text);

    // Wires one entity's EventHandlerComponent to this bridge. Call once per
    // actor as it's created -- see DamageTextSystem::Subscribe's own doc
    // comment for the full rationale, identical here.
    void Subscribe(Entity actor);

private:
    void OnHeal(Entity actor, AfterHealEvent& event);

    FloatingTextSystem* m_floating_text;
};

} // namespace psr
