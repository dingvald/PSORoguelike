#pragma once

#include "Engine/ECS/Entity.h"

namespace psr {

class FloatingTextSystem;
struct AfterDamageEvent;
struct AttackMissEvent;

// Bridges AfterDamageEvent/AttackMissEvent onto FloatingTextSystem: a white
// damage number floats up from the target's tile for every landed hit, a
// gray "Miss!" for every failed hit roll. Subscribed on every actor (player
// and every spawned enemy), same as CombatLogBridge and for the identical
// reason -- both events are dispatched at the attacker's own
// EventHandlerComponent (see DamageEvent.h), so damage/misses the player
// receives only reach this class through the attacking enemy's own
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
    void OnMiss(Entity actor, AttackMissEvent& event);

    FloatingTextSystem* m_floating_text;
};

} // namespace psr
