#pragma once

#include "Engine/ECS/Entity.h"

namespace psr {

// Scales base_cost by actor's ActorComponent::movement_speed/act_speed
// (100 == unmodified, matching every action's flat base cost today) --
// missing ActorComponent simply contributes nothing (baseline speed 100),
// same "safe to call for any entity" contract as ComputeEffectiveStats.
int EffectiveMoveCost(Entity actor, int base_cost);
int EffectiveActCost(Entity actor, int base_cost);

} // namespace psr
