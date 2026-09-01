#include "States/AnimationState.h"

#include "Components/TweenComponent.h"
#include "Systems/TweenSystem.h"

namespace psr {

StateTransition AnimationState::Update(GameplayContext& context, float delta_time)
{
    UpdateTweens(context.registry, delta_time);
    return context.registry.Any<TweenComponent>() ? StateTransition::None() : StateTransition::Pop();
}

} // namespace psr
