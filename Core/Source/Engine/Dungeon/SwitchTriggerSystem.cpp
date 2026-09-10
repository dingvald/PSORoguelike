#include "Engine/Dungeon/SwitchTriggerSystem.h"

#include "Engine/Dungeon/DoorComponent.h"
#include "Engine/Dungeon/DoorUnlock.h"
#include "Engine/Dungeon/SwitchComponent.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

namespace psr {

SwitchTriggerSystem::SwitchTriggerSystem(Registry& registry, Grid& grid) : m_registry(&registry), m_grid(&grid) {}

void SwitchTriggerSystem::Subscribe(Entity tracked)
{
    EventHandlerComponent& events = tracked.GetOrEmplace<EventHandlerComponent>();
    events.Subscribe<AfterMoveEvent, SwitchTriggerSystem>(
        [this](Entity actor, AfterMoveEvent& event) { OnAfterMove(actor, event); });
}

void SwitchTriggerSystem::OnAfterMove(Entity, AfterMoveEvent& event)
{
    for (entt::entity occupant : m_grid->GetEntities(event.to))
    {
        SwitchComponent* switch_component = m_registry->TryGetComponent<SwitchComponent>(occupant);
        if (!switch_component || switch_component->activated)
            continue;

        switch_component->activated = true;

        if (!m_registry->IsValid(switch_component->target_door))
            continue;
        DoorComponent* door = m_registry->TryGetComponent<DoorComponent>(switch_component->target_door);
        if (!door)
            continue;
        if (--door->remaining_switches <= 0)
            UnlockDoor(*m_registry, *m_grid, switch_component->target_door);
    }
}

} // namespace psr
