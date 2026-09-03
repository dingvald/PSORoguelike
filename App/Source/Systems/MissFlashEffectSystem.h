#pragma once

#include "Engine/ECS/Entity.h"

#include <entt/entt.hpp>

namespace psr {

class VisualEffectSystem;
struct AttackMissEvent;

// Bridges AttackMissEvent onto VisualEffectSystem: when the player
// specifically is the one missed, flashes a short-lived sprite overlay on
// the player's tile that fades out -- PSO's "block" flourish layered on top
// of DamageTextSystem's existing gray "Miss!" text, which fires for either
// direction. Subscribed on every actor (player and every spawned enemy),
// same as DamageTextSystem and for the identical reason -- AttackMissEvent
// is dispatched at the attacker's own EventHandlerComponent (see
// DamageEvent.h), so a miss the player receives only reaches this class
// through the attacking enemy's own subscription, not the player's.
class MissFlashEffectSystem
{
public:
    MissFlashEffectSystem(VisualEffectSystem& visual_effects, entt::entity player);

    // Wires one entity's EventHandlerComponent to this bridge. Call once per
    // actor as it's created -- see DamageTextSystem::Subscribe's own doc
    // comment for the full rationale, identical here.
    void Subscribe(Entity actor);

private:
    void OnMiss(Entity actor, AttackMissEvent& event);

    VisualEffectSystem* m_visual_effects;
    entt::entity m_player;
};

} // namespace psr
