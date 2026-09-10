#pragma once

#include "Engine/Actions/IAction.h"
#include "Engine/World/Grid.h"
#include "Items/AffixLibrary.h"

#include <random>

namespace psr {

// Advances one in-flight technique projectile (ProjectileComponent) by one
// hop per Perform() call -- the projectile-side counterpart to
// TechniqueAction's spawn-time branch (see ProjectileComponent.h). Stateless:
// every hop reads everything it needs off the acting entity's own
// ProjectileComponent/Position, so one long-lived instance is reused for
// every projectile actor, the same way TurnCoordinator's own
// m_default_npc_action/m_forced_wait_action are -- installed via
// GameplayLayer's SetNpcDecision wrapper, ahead of the real AI decision.
//
// Each hop moves the entity's logical Position/Grid membership instantly
// and queues a purely cosmetic glide Tween, same idiom as MoveAction. After
// every hop, resolves against whatever occupies the tile just landed on
// (Combat/ProjectileImpact.h's ResolveProjectileImpact) -- checked live per
// hop rather than deferred to path completion, so a target that only shares
// a tile with the projectile mid-flight still gets hit. A non-piercing bolt
// is destroyed as soon as it lands a hostile hit; a piercing one keeps
// going and can resolve against every tile in its path. Either way, the
// projectile entity is destroyed once it stops (impact or path exhausted).
//
// This only catches the case where the *projectile* moves onto an occupied
// tile. The reverse -- an actor walking onto a tile a projectile is
// currently sitting on between its own hops -- is caught symmetrically by
// MoveAction, which runs the same ResolveProjectileImpact check against any
// projectile already on the tile it just moved an actor onto.
class ProjectileAdvanceAction : public IAction
{
public:
    ProjectileAdvanceAction(Grid& grid, const AffixLibrary& affixes, std::mt19937& rng);

    ActionResult Perform(Entity actor) override;

private:
    Grid* m_grid;
    const AffixLibrary* m_affixes;
    std::mt19937* m_rng;
};

} // namespace psr
