#pragma once

#include "Engine/ECS/Entity.h"

namespace psr {

class FloatingTextSystem;
struct AfterDamageEvent;

// Bridges AfterDamageEvent onto FloatingTextSystem: a white damage number
// floats up from the target's tile for every landed hit. Subscribed on
// every actor (player and every spawned enemy), same as CombatLogBridge and
// for the identical reason -- AfterDamageEvent is dispatched at the
// attacker's own EventHandlerComponent (see DamageEvent.h), so damage the
// player receives only reaches this class through the attacking enemy's own
// subscription, not the player's.
class DamageTextSystem
{
public:
    explicit DamageTextSystem(FloatingTextSystem& floating_text);

    // Wires one entity's EventHandlerComponent to this bridge. Call once per
    // actor as it's created -- see CombatLogBridge::Subscribe's own doc
    // comment for the full rationale, identical here.
    void Subscribe(Entity actor);

private:
    void OnDamage(Entity actor, AfterDamageEvent& event);

    FloatingTextSystem* m_floating_text;
};

} // namespace psr
