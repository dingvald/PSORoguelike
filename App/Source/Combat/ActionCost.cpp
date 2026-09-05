#include "Combat/ActionCost.h"

#include "Components/ActorComponent.h"

#include <algorithm>

namespace psr {

namespace {
    int ScaleCost(int base_cost, int speed) { return std::max(1, base_cost * 100 / speed); }
} // namespace

int EffectiveMoveCost(Entity actor, int base_cost)
{
    const ActorComponent* stats = actor.TryGet<ActorComponent>();
    return ScaleCost(base_cost, stats ? stats->movement_speed : 100);
}

int EffectiveActCost(Entity actor, int base_cost)
{
    const ActorComponent* stats = actor.TryGet<ActorComponent>();
    return ScaleCost(base_cost, stats ? stats->act_speed : 100);
}

} // namespace psr
