#include "Systems/TweenSystem.h"

#include "Components/TweenComponent.h"

#include <functional>
#include <vector>

namespace psr {

void UpdateTweens(Registry& registry, float delta_time)
{
    std::vector<std::function<void()>> completions;
    std::vector<entt::entity> to_clear;

    registry.Each<TweenComponent>(
        [&](entt::entity entity, TweenComponent& tween_component)
        {
            float remaining = delta_time;
            while (!tween_component.queue.empty())
            {
                Tween& active = tween_component.queue.front();
                const float time_needed = active.duration - active.elapsed;
                if (remaining < time_needed)
                {
                    active.elapsed += remaining;
                    break;
                }
                remaining -= time_needed;
                if (active.on_completion)
                    completions.push_back(std::move(active.on_completion));
                tween_component.queue.erase(tween_component.queue.begin());
            }
            if (tween_component.queue.empty())
                to_clear.push_back(entity);
        });

    // Fired only after the Each pass completes -- a completion can dispatch
    // damage/death events that destroy entities (loot drops included), which
    // would otherwise invalidate the view mid-iteration (Each's own contract:
    // don't create/destroy entities from inside func).
    for (std::function<void()>& completion : completions)
        completion();

    // Re-validated rather than removed unconditionally: a completion could in
    // principle queue a follow-up Tween on an entity already collected here.
    for (entt::entity entity : to_clear)
    {
        if (!registry.IsValid(entity))
            continue;
        TweenComponent* tween_component = registry.TryGetComponent<TweenComponent>(entity);
        if (tween_component && tween_component->queue.empty())
            registry.Remove<TweenComponent>(entity);
    }
}

} // namespace psr
