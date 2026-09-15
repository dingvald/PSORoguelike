#pragma once

#include "Combat/Faction.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// Overrides Hostility.h's GetFaction fallback (PlayerControlledComponent ->
// Player, AiComponent -> Enemy, else Neutral) for a prefab that needs to
// deviate from it -- neutral wildlife that still chases via AiComponent,
// a summoned ally, or an enemy prefab reused as a friendly NPC. Absent is the
// common case; most content authors never need this card.
struct FactionComponent
{
    Faction faction = Faction::Neutral;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<FactionComponent>("faction").Data<&FactionComponent::faction>("faction");
    }
};

} // namespace psr
