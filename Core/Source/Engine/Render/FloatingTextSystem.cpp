#include "Engine/Render/FloatingTextSystem.h"

namespace psr {

void FloatingTextSystem::Spawn(Vec2 origin_tile, std::string text, Color color, Vec2f direction, float speed,
                               float duration)
{
    m_instances.push_back(
        FloatingTextInstance{origin_tile, Vec2f{}, direction, speed, std::move(text), color, duration, 0.0f});
}

void FloatingTextSystem::Update(float delta_time)
{
    for (FloatingTextInstance& instance : m_instances)
    {
        instance.offset = instance.offset + instance.direction * instance.speed * delta_time;
        instance.elapsed += delta_time;
    }

    std::erase_if(m_instances,
                  [](const FloatingTextInstance& instance) { return instance.elapsed >= instance.duration; });
}

} // namespace psr
