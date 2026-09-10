#include "Engine/ECS/LifetimeSystem.h"

#include "Engine/ECS/LifetimeComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/World/Grid.h"

#include <vector>

namespace psr {

void LifetimeSystem::Tick()
{
    std::vector<entt::entity> expired;
    m_registry->Each<LifetimeComponent>(
        [&expired](entt::entity entity, LifetimeComponent& lifetime)
        {
            if (--lifetime.remaining_turns <= 0)
                expired.push_back(entity);
        });

    for (entt::entity entity : expired)
    {
        if (const Position* position = m_registry->TryGetComponent<Position>(entity))
            m_registry->GetGrid().RemoveEntity(position->tile, entity);
        m_registry->DestroyEntity(entity);
    }
}

} // namespace psr
