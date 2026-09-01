#pragma once

#include "Engine/Actions/IAction.h"
#include "Engine/Math/Vec2.h"
#include "Engine/World/Grid.h"
#include "Items/AffixLibrary.h"

#include <random>

namespace psr {

// Resolves a melee/ranged attack from actor's equipped weapon
// (EquipmentComponent::weapon) toward direction, per the weapon's
// WeaponRangeShape. A free no-op (cost 0) if actor has no weapon equipped,
// or if no hostile HealthComponent-carrying occupant is found in range --
// otherwise costs kAttackCost. See MoveAction, whose bump-into-a-hostile
// fallback is what constructs this.
//
// Damage is not applied here: Perform() only finds targets, then queues two
// Tweens on the actor -- a lunge toward direction, and a return to {0,0} --
// with the actual hit-resolution loop (hit rolls, damage, status) captured
// as the first Tween's on_completion callback, so it fires once the lunge
// visually reaches the target rather than the instant the attack is
// declared. This blocks the whole turn queue for the lunge's duration (see
// AnimationState) -- a target can't flee or die from something else, and no
// other actor can act, before that callback runs.
class AttackAction : public IAction
{
public:
    static constexpr int kAttackCost = 100;
    static constexpr float kLungeDistance = 0.35f;     // tile-fraction step toward the target at the lunge's peak
    static constexpr float kLungeOutDuration = 0.12f;
    static constexpr float kLungeBackDuration = 0.12f;

    AttackAction(Grid& grid, const AffixLibrary& affixes, Vec2 direction, std::mt19937& rng);

    ActionResult Perform(Entity actor) override;

private:
    Grid* m_grid;
    const AffixLibrary* m_affixes;
    Vec2 m_direction;
    std::mt19937* m_rng;
};

} // namespace psr
