#pragma once

#include "Components/WeaponComponent.h" // WeaponRangeShape
#include "Engine/ECS/Entity.h"
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
// direction. Shared by WeaponAttackAction (WeaponComponent's own range_shape/range)
// and PhotonArtAction/TechniqueAction (PhotonArt/Technique's own fields of the
// same name) -- originally WeaponAttackAction's file-local helper, lifted out once a
// second and third call site needed the identical geometry against a
// different owning struct each time.
std::vector<Vec2> ResolveTargetTiles(const Grid& grid, Registry& registry, Vec2 origin, Vec2 direction,
                                     WeaponRangeShape shape, int range);

// Snaps an arbitrary tile offset (e.g. a TargetSquare-picked tile minus the
// caster's own tile) to the nearest of the 8 surrounding unit directions
// (4 cardinal + 4 diagonal), so PhotonArtAction/TechniqueAction/
// WeaponAttackAction can feed a freely-picked target through the same
// direction-based ResolveTargetTiles every other shape already uses --
// WeaponRangeShape::Cone3 in particular expects a diagonal direction to flank
// correctly (see ResolveTargetTiles's own comment). {0,0} in, {0,0} out (the
// SelfTarget case -- callers check for this separately to skip tile
// resolution entirely). An offset within 22.5 degrees of an axis snaps
// cardinal; otherwise it snaps to the nearest diagonal.
Vec2 SnapToDirection(Vec2 offset);

// The tile sequence a Line/SingleTarget-shaped projectile travels from origin
// (exclusive) toward direction, one tile per turn -- see ProjectileComponent.h/
// ProjectileAdvanceAction.h. Always stops at grid edge or a wall (a
// BlocksMovementComponent occupant with no HealthComponent, same as
// ResolveTargetTiles's own Line case). When pierces is false, additionally
// stops right after including the first tile that has any HealthComponent-
// bearing occupant (hostile or not -- the bolt physically collides with
// whatever's there first; hostility is checked separately at impact, before
// damage applies). When pierces is true, never stops early for a creature --
// it always reaches the same tiles ResolveTargetTiles's Line case would for
// an instant cast, just resolved once travel finishes rather than
// immediately.
std::vector<Vec2> BuildProjectilePath(const Grid& grid, Registry& registry, Vec2 origin, Vec2 direction, int range,
                                      bool pierces);

// TargetingMode::TargetSquare's own resolution: SingleTarget hits target
// itself (whatever the player picked, anywhere within the reachable range
// TargetSelectionState already validated -- unlike ResolveTargetTiles's
// SingleTarget case, which only ever reaches the one tile adjacent to
// origin); Line walks a Bresenham ray from origin through target, continued
// past it for the full range tiles so target conveys aim, not distance --
// this also produces the correct 8 unit directions when target happens to be
// one of origin's 8 neighbours (Directional mode's own cursor never leaves
// that set), so callers can use this for every TargetingMode uniformly for
// these two shapes. Cone3/Surrounding fall back to ResolveTargetTiles via
// SnapToDirection(target - origin): no authored content combines
// TargetSquare with either shape, and their flanking/cardinal math assumes
// one of the 8 unit directions.
std::vector<Vec2> ResolveTargetTilesToward(const Grid& grid, Registry& registry, Vec2 origin, Vec2 target,
                                           WeaponRangeShape shape, int range);

// BuildProjectilePath's own target-tile counterpart: walks the same
// Bresenham ray ResolveTargetTilesToward's Line case uses instead of a fixed
// unit direction, so a projectile can travel toward any angle, not just the
// 8 SnapToDirection unit vectors.
std::vector<Vec2> BuildProjectilePathToward(const Grid& grid, Registry& registry, Vec2 origin, Vec2 target,
                                            int range, bool pierces);

// Bresenham tile-line test: true if no wall tile (a BlocksMovementComponent
// occupant with no HealthComponent, same definition ResolveTargetTiles/
// BuildProjectilePath use) lies strictly between from and to. Endpoints
// themselves are never tested, so standing next to or on a wall's tile
// doesn't block sight to/from that tile. Used by EnemyAiSystem's detection
// gate so a larger detection_range doesn't see through walls.
bool HasLineOfSight(const Grid& grid, Registry& registry, Vec2 from, Vec2 to);

// True if tile is a viable step for actor: empty of BlocksMovementComponent
// occupants, or its (sole expected) blocking occupant is a hostile entity
// with a HealthComponent -- MoveAction's own bump fallback is what turns
// stepping there into a WeaponAttackAction instead of a plain move. Out-of-
// bounds is never viable. Shared by EnemyAiSystem's movement decisions and
// FindPath's walkability predicate (see Pathfinder.h) for both AI and
// player-driven click-to-move.
bool IsWalkableStep(const Grid& grid, Registry& registry, Entity actor, Vec2 tile);

} // namespace psr
