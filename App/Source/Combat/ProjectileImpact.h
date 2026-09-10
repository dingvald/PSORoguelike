#pragma once

#include "Engine/ECS/Registry.h"
#include "Engine/Math/Vec2.h"
#include "Engine/World/Grid.h"
#include "Items/AffixLibrary.h"

#include <random>

namespace psr {

struct ProjectileComponent;

// Resolves one in-flight projectile against whatever occupies `tile` right
// now -- fresh hit-roll/damage against the tile's current occupants,
// dispatched as ProjectileComponent::source so combat log/lifesteal/
// OnHitEffectSystem all attribute it to the original caster, not the
// projectile. Shared by ProjectileAdvanceAction (checked against the tile a
// projectile just hopped onto) and MoveAction (checked against a tile an
// actor just walked onto, so a projectile sitting there between its own
// hops still connects with whoever steps into it -- the symmetric case).
// Returns whether a hostile target was found on the tile, whether or not
// the hit roll/effect actually landed -- callers use this to decide
// whether a non-piercing projectile stops here.
bool ResolveProjectileImpact(Registry& registry, const Grid& grid, const AffixLibrary& affixes, std::mt19937& rng,
                             const ProjectileComponent& projectile, Vec2 tile);

} // namespace psr
