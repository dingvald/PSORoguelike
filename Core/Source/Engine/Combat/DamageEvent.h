#pragma once

#include "Engine/ECS/Entity.h"

namespace psr {

// Dispatched by AttackAction/TechniqueAction/PhotonArtAction to the acting
// entity's own EventHandlerComponent (Entity::Dispatch) around every HP
// mutation they apply to a target -- never to the target itself, only ever
// to the actor, per the per-entity event bus's "acting entity" convention.
// Shared by all three actions rather than three near-duplicate structs: a
// listener (e.g. CombatLogBridge) only cares "who hit whom for how much,"
// not which action caused it.
struct BeforeDamageEvent
{
    Entity target;
    int incoming_damage = 0; // mutable: a future mitigation handler could adjust this before it's applied
};

struct AfterDamageEvent
{
    Entity target;
    int amount = 0;
    bool is_critical = false;
    bool target_defeated = false;
};

// Dispatched by AttackAction/TechniqueAction/PhotonArtAction to the acting
// entity, same "acting entity" convention as Before/AfterDamageEvent above --
// fired instead of Before/AfterDamageEvent when a hit roll (ComputeHitChance)
// fails, so a listener never sees both events for the same swing.
struct AttackMissEvent
{
    Entity target;
};

// Dispatched by a damage source (AttackAction/PhotonArtAction/TechniqueAction/
// StatusEffectApplication) to the *target itself* -- Entity::Dispatch, i.e.
// "self", unlike Before/AfterDamageEvent above which always go to the actor
// -- once a final amount has been resolved (post BeforeDamageEvent
// mitigation). HealthSystem is the sole intended handler: it applies amount
// to the target's own HealthComponent, dispatches AfterDamageEvent back at
// source on its behalf (so that still fires before the entity can be
// destroyed -- see HealthSystem.h), and if that reduces current_hp to 0,
// dispatches DeathEvent to the target. Damage sources never touch
// HealthComponent directly any more -- see AttackAction::Perform for the
// call-site pattern.
struct IncomingDamageEvent
{
    Entity source; // who to dispatch AfterDamageEvent at once this is applied
    int amount = 0;
    bool is_critical = false;
};

// Dispatched by HealthSystem to an entity whose HealthComponent::current_hp
// it just reduced to 0 (self-dispatched, same convention as
// IncomingDamageEvent). DeathSystem is the sole intended handler: it removes
// the entity from the Grid (if placed on one) and destroys it. Carries no
// data -- "self" (the handler's Entity parameter) is the entity that died;
// nothing downstream needs more than that yet.
struct DeathEvent
{
};

} // namespace psr
