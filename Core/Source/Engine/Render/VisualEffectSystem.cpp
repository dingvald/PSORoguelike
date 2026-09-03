#include "Engine/Render/VisualEffectSystem.h"

#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

#include <algorithm>
#include <cmath>

namespace psr {

VisualEffectSystem::VisualEffectSystem(Registry& registry, Grid& grid,
                                       std::function<void(entt::entity, std::uint8_t)> alpha_sink)
    : m_registry(&registry), m_grid(&grid), m_alpha_sink(std::move(alpha_sink))
{
}

entt::entity VisualEffectSystem::Spawn(std::uint32_t prefab_id, Vec2 tile, float duration, EasingCurve easing,
                                       std::uint8_t start_alpha, std::uint8_t end_alpha)
{
    if (!m_registry->HasPrefab(prefab_id))
        return entt::null;

    const entt::entity entity = m_registry->CreateEntity(prefab_id);
    m_grid->AddEntity(tile, entity);
    m_instances.push_back(VisualEffectInstance{entity, tile, duration, 0.0f, easing, start_alpha, end_alpha});
    m_alpha_sink(entity, start_alpha);
    return entity;
}

void VisualEffectSystem::Update(float delta_time)
{
    for (VisualEffectInstance& instance : m_instances)
    {
        instance.elapsed += delta_time;
        const float t = instance.duration > 0.0f ? std::clamp(instance.elapsed / instance.duration, 0.0f, 1.0f) : 1.0f;
        const float eased = Ease(instance.easing, t);
        const float alpha = static_cast<float>(instance.start_alpha) +
                            (static_cast<float>(instance.end_alpha) - static_cast<float>(instance.start_alpha)) * eased;
        m_alpha_sink(instance.entity, static_cast<std::uint8_t>(std::round(alpha)));
    }

    std::erase_if(m_instances,
                  [this](const VisualEffectInstance& instance)
                  {
                      if (instance.elapsed < instance.duration)
                          return false;
                      m_grid->RemoveEntity(instance.tile, instance.entity);
                      m_registry->DestroyEntity(instance.entity);
                      return true;
                  });
}

} // namespace psr
