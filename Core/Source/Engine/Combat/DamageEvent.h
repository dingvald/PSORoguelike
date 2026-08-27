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
    bool target_defeated = false;
};

} // namespace psr
