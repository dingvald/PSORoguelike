#pragma once

#include "Engine/Input/ActionMap.h"
#include "Engine/World/Grid.h"

namespace psr {

// Default 4-directional arrow-key movement + Space-to-wait bindings for a
// TurnCoordinator, tied to `grid` (the live dungeon Grid the returned
// MoveActions move entities through). Diagonal movement is deliberately not
// bound yet -- nothing in the GDD commits to it, and 4-directional is the
// simpler default to start from.
ActionMap<int> CreateDefaultKeyBindings(Grid& grid);

} // namespace psr
