#pragma once

#include "Components/TeleporterComponent.h"

#include <optional>

namespace psr {

// Published every frame by GameplayLayer while the dungeon scene is active
// (mirrors HubInteractionPromptMessage.h exactly, dungeon-scene sibling) --
// nullopt hides HudLayer's "Press SPACE to ..." prompt, a value shows the one
// matching the player's current tile (see Missions/TeleporterInteraction.h's
// FindTeleporterAt). Also published once with nullopt on any transition away
// from the dungeon, so the prompt doesn't linger once per-frame publishing
// stops.
struct TeleporterPromptMessage
{
    std::optional<TeleporterDestination> destination;
};

} // namespace psr
