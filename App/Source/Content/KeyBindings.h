#pragma once

#include "Engine/Input/ActionMap.h"
#include "Engine/World/Grid.h"
#include "Items/AffixLibrary.h"

#include <random>

namespace psr {

// Default 4-directional arrow-key movement + Space-to-wait bindings for a
// TurnCoordinator, tied to `grid` (the live dungeon Grid the returned
// MoveActions move entities through). `affixes`/`rng` are threaded through
// to each MoveAction for its bump-to-attack fallback (see MoveAction.h) --
// callers own both for as long as the returned ActionMap is in use. Diagonal
// movement is deliberately not bound yet -- nothing in the GDD commits to
// it, and 4-directional is the simpler default to start from.
ActionMap<int> CreateDefaultKeyBindings(Grid& grid, const AffixLibrary& affixes, std::mt19937& rng);

} // namespace psr
