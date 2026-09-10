#pragma once

#include "Components/TeleporterComponent.h"
#include "Engine/Math/Vec2.h"

#include <optional>

namespace psr {

class Registry;
class Grid;

// The TeleporterDestination of the first entity on tile that carries a
// TeleporterComponent, or nullopt if none does -- mirrors Hub/HubInteraction.h's
// FindInteractableAt exactly, one scene-swap-triggering concept, one lookup.
// Used both for GameplayLayer's per-frame HUD prompt and its dungeon-scene
// Space-key dispatch.
std::optional<TeleporterDestination> FindTeleporterAt(const Registry& registry, const Grid& grid, Vec2 tile);

} // namespace psr
