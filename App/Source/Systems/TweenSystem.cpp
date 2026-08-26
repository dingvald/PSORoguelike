#include "Systems/TweenSystem.h"

#include "Components/TweenComponent.h"

#include <vector>

namespace psr {

void UpdateTweens(Registry& registry, float delta_time)
{
    std::vector<entt::entity> finished;

    registry.Each<TweenComponent>(
        [&](entt::entity entity, TweenComponent& tween)
        {
            tween.elapsed += delta_time;
            if (tween.elapsed >= tween.duration)
                finished.push_back(entity);
        });

    for (entt::entity entity : finished)
        registry.Remove<TweenComponent>(entity);
}

} // namespace psr
