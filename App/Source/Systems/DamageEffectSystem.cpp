#include "Systems/DamageEffectSystem.h"

#include "Components/DamageFlashComponent.h"
#include "Components/RenderableComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Registry.h"

#include <vector>

namespace psr {

namespace {
    constexpr float kDamageFlashDuration = 0.25f; // seconds
    constexpr int kDamageFlashCount = 2;
    constexpr float kDamageFlashPeriod = kDamageFlashDuration / (kDamageFlashCount * 2);
    constexpr Color kDamageFlashColor{255, 255, 255, 255};
} // namespace

void DamageEffectSystem::Subscribe(Entity actor)
{
    EventHandlerComponent& events = actor.GetOrEmplace<EventHandlerComponent>();
    events.Subscribe<AfterDamageEvent, DamageEffectSystem>([](Entity entity, AfterDamageEvent& event)
                                                            { OnDamage(entity, event); });
}

void DamageEffectSystem::OnDamage(Entity /*actor*/, AfterDamageEvent& event)
{
    if (event.amount <= 0)
        return;

    RenderableComponent* renderable = event.target.TryGet<RenderableComponent>();
    if (!renderable)
        return;

    DamageFlashComponent* flash = event.target.TryGet<DamageFlashComponent>();
    if (!flash)
    {
        flash = &event.target.Emplace<DamageFlashComponent>();
        flash->base_color_1 = renderable->color_1;
        flash->base_color_2 = renderable->color_2;
    }
    flash->elapsed = 0.0f; // re-hit mid-flash: restart the cycle, keep the original baseline
}

void DamageEffectSystem::Update(Registry& registry, float delta_time)
{
    std::vector<entt::entity> to_clear;

    registry.Each<DamageFlashComponent>(
        [&](entt::entity entity, DamageFlashComponent& flash)
        {
            flash.elapsed += delta_time;
            RenderableComponent* renderable = registry.TryGetComponent<RenderableComponent>(entity);
            if (!renderable || flash.elapsed >= kDamageFlashDuration)
            {
                if (renderable)
                {
                    renderable->color_1 = flash.base_color_1;
                    renderable->color_2 = flash.base_color_2;
                }
                to_clear.push_back(entity);
                return;
            }

            const int half_cycle = static_cast<int>(flash.elapsed / kDamageFlashPeriod);
            const bool flash_on = half_cycle % 2 == 0;
            renderable->color_1 = flash_on ? kDamageFlashColor : flash.base_color_1;
            renderable->color_2 = flash_on ? kDamageFlashColor : flash.base_color_2;
        });

    for (entt::entity entity : to_clear)
        registry.Remove<DamageFlashComponent>(entity);
}

} // namespace psr
