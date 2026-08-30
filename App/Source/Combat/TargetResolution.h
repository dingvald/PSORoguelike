#pragma once

#include "Components/WeaponComponent.h" // WeaponRangeShape
#include "Engine/ECS/Registry.h"
#include "Engine/Math/Vec2.h"
#include "Engine/World/Grid.h"

#include <vector>

namespace psr {

// Resolves which tiles `shape` reaches from origin toward direction:
// SingleTarget is the one adjacent tile; Line pierces up to `range` tiles,
// stopping at grid edge or a wall (a BlocksMovementComponent occupant with no
// HealthComponent); Cone3 is the forward tile plus its two perpendicular
// neighbours; Surrounding is all four cardinal-adjacent tiles, ignoring
// direction. Shared by AttackAction (WeaponComponent's own range_shape/range)
// and PhotonArtAction/TechniqueAction (PhotonArt/Technique's own fields of the
// same name) -- originally AttackAction's file-local helper, lifted out once a
// second and third call site needed the identical geometry against a
// different owning struct each time.
std::vector<Vec2> ResolveTargetTiles(const Grid& grid, Registry& registry, Vec2 origin, Vec2 direction,
                                     WeaponRangeShape shape, int range);

// Snaps an arbitrary tile offset (e.g. a TargetSquare-picked tile minus the
// caster's own tile) to the nearest cardinal unit direction, so
// PhotonArtAction/TechniqueAction can feed a freely-picked target through the
// same direction-based ResolveTargetTiles every other shape already uses.
// {0,0} in, {0,0} out (the SelfTarget case -- callers check for this
// separately to skip tile resolution entirely). Ties between axes favour the
// horizontal axis.
Vec2 SnapToCardinalDirection(Vec2 offset);

} // namespace psr
