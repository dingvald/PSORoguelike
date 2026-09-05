#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// An actor's persisted scheduling energy (ap) plus the two speed stats that
// scale action costs (see Combat/ActionCost.h) -- ap is the TurnQueue source
// of truth (TurnQueue itself only holds the transient scheduling copy,
// resynced via Requeue after every turn); presence/absence of this component
// is what drives TurnQueue membership (see TurnCoordinator). ap rides along
// on the same authorable component as movement_speed/act_speed (Component<T>
// only has a whole-component authorable flag, not per-field), but
// PrefabEditorLayer's Actor card omits its field and Read/WriteActorBody
// never round-trip it through JSON, so content can't set it -- same as
// content never touched it when it lived alone on the old EnergyComponent.
struct ActorComponent
{
    int ap = 0;
    int movement_speed = 100;
    int act_speed = 100;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<ActorComponent>("actor")
            .Data<&ActorComponent::ap>("ap")
            .Data<&ActorComponent::movement_speed>("movement_speed")
            .Data<&ActorComponent::act_speed>("act_speed");
    }
};

} // namespace psr
