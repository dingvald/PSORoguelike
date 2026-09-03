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
// and queues a purely cosmetic glide Tween, same idiom as MoveAction. Once
// the path is exhausted, resolves the hit (fresh hit-roll/damage against
// whatever occupies the impact tile(s) now, dispatched as
// ProjectileComponent::source so combat log/lifesteal/OnHitEffectSystem all
// attribute it to the original caster, not the projectile) and destroys the
// projectile entity.
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
