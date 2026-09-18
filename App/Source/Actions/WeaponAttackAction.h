#pragma once

#include "Engine/Actions/IAction.h"
#include "Engine/Math/Vec2.h"
#include "Engine/World/Grid.h"
#include "Items/AffixLibrary.h"

#include <optional>
#include <random>

namespace psr {

// Resolves the acting entity's equipped weapon's normal attack -- the single
// action class for both a bump-into-hostile swing (MoveAction's fallback,
// direction fixed at construction from the move offset) and the hotbar's
// Normal Attack slot (GameplayLayer::TryActivateSlot, direction omitted --
// resolved from SelectedTargetComponent once TargetSelectionState confirms a
// tile, same "stateless w.r.t. target" convention PhotonArtAction/
// TechniqueAction already use). There is deliberately no separate
// bump-specific action class.
//
// A free no-op (cost 0) if the actor has no weapon equipped, the attack was
// cancelled (e.g. Shocked), or -- when direction is a fixed value (a bump)
// -- the weapon fires a projectile: a ranged weapon's attack must be fired
// explicitly through the hotbar's tile-select targeting (direction ==
// nullopt), never by simply walking into a hostile. Otherwise costs
// kWeaponAttackCost.
//
// A melee resolution (WeaponComponent::fires_projectile == false) finds
// targets via WeaponRangeShape and, for each one hit, applies knockback (a
// 1-tile push away from the attacker) directly; hit-stun instead rides on
// the IncomingDamageEvent each hit already dispatches, applied later by
// TurnCoordinator's own AfterDamageEvent subscription (see
// TurnCoordinator::Subscribe) rather than here -- this action holds no
// TurnCoordinator& of its own. Damage is not applied immediately: Perform()
// only finds targets, then queues two Tweens on the actor -- a lunge toward
// direction, and a return to {0,0} -- with the actual hit-resolution loop
// captured as the first Tween's on_completion callback, so it fires once the
// lunge visually reaches the target.
//
// A ranged resolution (WeaponComponent::fires_projectile == true, direction
// == nullopt) spawns a real travelling ProjectileComponent entity instead,
// the same mechanism TechniqueAction's own projectile branch uses, with
// physical (ATP-vs-DFP, race-bonus, crit) damage rather than a Technique's
// MST-based magic formula.
//
// is_special_attack (the hotbar's Special Attack slot, HotbarSlotType::
// SpecialAttack -- distinct from a bump/Normal Attack swing) is a free no-op
// if the equipped weapon has no elemental flavor (WeaponComponent::element ==
// Element::None): there is no "special" to execute without a prefix granting
// one. Otherwise it swings exactly like Normal Attack, except the landed
// hit's elemental proc (see Combat/StatusEffectHooks.h's
// RollElementalDamageBonus) is guaranteed (100% pre-resistance) rather than
// status_chance_percent-rolled, and its bonus damage is doubled -- a
// player-triggered guarantee of what Normal Attack only sometimes procs.
class WeaponAttackAction : public IAction
{
public:
    static constexpr int kWeaponAttackCost = 100;
    static constexpr float kLungeDistance = 0.65f; // tile-fraction step toward the target at the lunge's peak
    static constexpr float kLungeOutDuration = 0.10f;
    static constexpr float kLungeBackDuration = 0.12f;

    // direction: a fixed swing direction (MoveAction's bump fallback).
    // nullopt (default): resolve from the acting entity's own
    // SelectedTargetComponent instead (the hotbar's Normal Attack/Special
    // Attack slots).
    explicit WeaponAttackAction(Grid& grid, const AffixLibrary& affixes, std::mt19937& rng,
                               std::optional<Vec2> direction = std::nullopt, bool is_special_attack = false);

    ActionResult Perform(Entity actor) override;

private:
    Grid* m_grid;
    const AffixLibrary* m_affixes;
    std::mt19937* m_rng;
    std::optional<Vec2> m_direction;
    bool m_is_special_attack;
};

} // namespace psr
