#pragma once

#include "Components/InteractableComponent.h"

#include <optional>

namespace psr {

// Published every frame by GameplayLayer while the hub scene is active (same
// "publish every frame regardless of change" convention TargetStateMessage/
// FloatingTextStateMessage already use) -- nullopt hides HudLayer's "Press
// SPACE to ..." prompt, a value shows the one matching the player's current
// tile (see Hub/HubInteraction.h's FindInteractableAt). Also published once
// with nullopt on any transition away from the hub, so the prompt doesn't
// linger once per-frame publishing stops.
struct HubInteractionPromptMessage
{
    std::optional<InteractionType> interaction_type;
};

} // namespace psr
