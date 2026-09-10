#pragma once

#include "Engine/Actions/MoveEvent.h"
#include "Engine/ECS/Entity.h"

namespace psr {

class Registry;
class Grid;

// Activates a SwitchComponent-tagged entity the moment an actor walks onto its tile --
// mirrors StatusEffectWorldMarkers::Subscribe's AfterMoveEvent-wiring idiom exactly, just
// reacting to the target tile instead of the mover's own status effects. Decrements the
// switch's target door's DoorComponent::remaining_switches and unlocks it (via
// DoorUnlock.h) once that reaches zero. Since a lock's switch and door can live in
// different placed pieces/rooms, this system has no room-index dependency at all -- it
// only ever needs the switch's own target_door handle.
class SwitchTriggerSystem
{
public:
    SwitchTriggerSystem(Registry& registry, Grid& grid);

    // Wires tracked's EventHandlerComponent to this instance. Only the
    // player needs this -- enemies don't trigger switches.
    void Subscribe(Entity tracked);

private:
    void OnAfterMove(Entity actor, AfterMoveEvent& event);

    Registry* m_registry;
    Grid* m_grid;
};

} // namespace psr
