#pragma once

#include "Combat/TechniqueLibrary.h"
#include "Engine/Actions/IAction.h"
#include "Engine/World/Grid.h"
#include "Items/AffixLibrary.h"

#include <cstdint>
#include <random>

namespace psr {

// Casts a specific weapon-granted Technique (technique_id, resolved into
// techniques) -- Force's TP-costed elemental spell equivalent of
// PhotonArtAction, reading its target from actor's SelectedTargetComponent
// the same way. Damage is MST-based (CombatMath::ComputeTechniqueDamage)
// rather than ATP-based. Technique carries no drain_percent field (unlike
// PhotonArt) -- EffectFamily::Drain still type-checks (the enum is shared)
// but resolves identically to Damage here for lack of an amount to size a
// restore by. On a directional hit, element/status_effect_id/
// status_chance_percent have a chance to inflict an ailment (see
// StatusEffectHooks.h's MaybeApplyElementalStatus); a Status-family cast
// applies its ailment unconditionally instead of dealing damage. No
// elemental resistance/multiplier table exists yet -- element is otherwise
// informational this pass.
//
// Same free-no-op/cost rules as PhotonArtAction, and the same TPComponent
// pool (see docs/GDD.md's "PP vs. TP (revised -- collapsed to one pool)" section): a missing weapon,
// a weapon that doesn't grant technique_id, or insufficient TP is a free
// no-op; otherwise TP is spent and the turn is consumed (kTechniqueCost)
// regardless of whether the cast connects.
class TechniqueAction : public IAction
{
public:
    static constexpr int kTechniqueCost = 100;

    TechniqueAction(Grid& grid, const TechniqueLibrary& techniques, const AffixLibrary& affixes,
                    std::uint32_t technique_id, std::mt19937& rng);

    ActionResult Perform(Entity actor) override;

private:
    Grid* m_grid;
    const TechniqueLibrary* m_techniques;
    const AffixLibrary* m_affixes;
    std::uint32_t m_technique_id;
    std::mt19937* m_rng;
};

} // namespace psr
