#pragma once

#include "Components/InteractableComponent.h"

#include <optional>

namespace psr {

// Published every frame by GameplayLayer while the hub scene is active (same
// "publish every frame regardless of change" convention TargetStateMessage/
// FloatingTextStateMessage already use) -- nullopt hides HudLayer's "Press
// SPACE to ..." prompt, a value shows the one matching the player's current
// tile (see Hub/HubInteraction.h's FindInteractableAt).
struct HubInteractionPromptMessage
{
    std::optional<InteractionType> interaction_type;
};

} // namespace psr
