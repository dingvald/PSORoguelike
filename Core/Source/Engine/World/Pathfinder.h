#pragma once

#include "Engine/Math/Vec2.h"

#include <functional>
#include <vector>

namespace psr {

class Grid;

// A* over grid's tiles, 8-directional (diagonal steps cost the same as
// cardinal -- see MoveAction::kMoveCost, a flat per-step cost regardless of
// direction -- so the search uses a uniform g-cost and a Chebyshev heuristic,
// not octile distance). is_walkable is queried for every candidate tile
// other than start (including goal); FindPath checks grid.Contains() itself
// first, so callers' predicates don't need to duplicate bounds logic. Corner
// -cutting past a diagonal's flanking tiles is not prevented, matching the
// game's existing single-step movement rules.
//
// Returns the path excluding start, in travel order, ending at goal --
// mirrors BuildProjectilePath's origin-exclusive convention. Empty if
// start == goal, goal is out of bounds, or goal is unreachable.
//
// Grid itself has no concept of walkability (it's dumb tile-occupant
// storage); is_walkable is how a caller injects what that means for its own
// component types without Core depending on them -- same split
// TargetResolution.h's HasLineOfSight already established for line-of-sight
// queries over the same Grid.
std::vector<Vec2> FindPath(const Grid& grid, Vec2 start, Vec2 goal, const std::function<bool(Vec2)>& is_walkable);

} // namespace psr
