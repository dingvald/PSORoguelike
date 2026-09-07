#pragma once

#include "Components/InteractableComponent.h"
#include "Engine/Math/Vec2.h"

#include <optional>

namespace psr {

class Registry;
class Grid;

// The InteractionType of the first entity on tile that carries an
// InteractableComponent, or nullopt if none does (an empty tile, or one
// with only non-interactable occupants). "First" follows Grid::GetEntities'
// own insertion order -- if a hub piece ever stamps more than one
// interactable prefab into the same cell (not done by this project's own
// content), the earliest-stamped one wins. Used both for GameplayLayer's
// per-frame HUD prompt and its Space-key dispatch, so there's exactly one
// place this resolution logic lives.
std::optional<InteractionType> FindInteractableAt(const Registry& registry, const Grid& grid, Vec2 tile);

} // namespace psr
