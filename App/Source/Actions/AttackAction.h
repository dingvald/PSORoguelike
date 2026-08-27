#pragma once

#include "Engine/Actions/IAction.h"
#include "Engine/Items/AffixLibrary.h"
#include "Engine/Math/Vec2.h"
#include "Engine/World/Grid.h"

#include <random>

namespace psr {

// Resolves a melee/ranged attack from actor's equipped weapon
// (EquipmentComponent::weapon) toward direction, per the weapon's
// WeaponRangeShape. A free no-op (cost 0) if actor has no weapon equipped,
// or if no hostile HealthComponent-carrying occupant is found in range --
// otherwise costs kAttackCost. See MoveAction, whose bump-into-a-hostile
// fallback is what constructs this.
class AttackAction : public IAction
{
public:
    static constexpr int kAttackCost = 100;

    AttackAction(Grid& grid, const AffixLibrary& affixes, Vec2 direction, std::mt19937& rng);

    ActionResult Perform(Entity actor) override;

private:
    Grid* m_grid;
    const AffixLibrary* m_affixes;
    Vec2 m_direction;
    std::mt19937* m_rng;
};

} // namespace psr
