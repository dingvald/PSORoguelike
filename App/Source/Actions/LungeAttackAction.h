#pragma once

#include "Engine/Actions/IAction.h"
#include "Engine/Math/Vec2.h"
#include "Engine/World/Grid.h"
#include "Items/AffixLibrary.h"

#include <cstdint>
#include <random>

namespace psr {

class VisualEffectSystem;

// A pounce-and-strike: moves actor one tile toward direction, then attacks
// whatever's on the tile beyond that (2 tiles from where actor started) --
// AiBehavior::KeepDistanceAndPounce's payoff for closing to PounceComponent::
// preferred_distance (which must equal kLungeRange for this to ever trigger;
// see EnemyAiSystem::DecideKeepDistanceAndPounce). direction must already be
// a true unit vector (one of the 8 cardinal/diagonal directions) pointing
// exactly at a target kLungeRange tiles away -- callers are expected to have
// verified alignment themselves (see the sign-based delta -> unit vector
// idiom WeaponAttackAction.cpp's own ApplyKnockback uses), since this class
// has no way to re-derive "aligned" from an arbitrary offset the way
// SnapToDirection would (and SnapToDirection would silently mis-aim an
// unaligned delta rather than reject it).
//
// Perform() itself only does the pounce -- for free (ActionResult(0)) -- and
// hands off to a fallback WeaponAttackAction (fixed direction) for the actual
// hit resolution; per ResolveAction's contract, only the fallback's own cost
// is ever charged, so a pounce + attack together cost exactly one base
// action, the same as a plain WeaponAttackAction alone. The pounce move
// deliberately bypasses MoveAction/BeforeMoveEvent -- a Confuse-style
// redirect firing between the pounce and the fixed-direction attack it feeds
// into would aim the strike somewhere the actor didn't actually land, so this
// models the pounce as part of the attack, not as an interruptible step.
class LungeAttackAction : public IAction
{
public:
    static constexpr int kPounceDistance = 1;
    static constexpr int kLungeRange = kPounceDistance * 2; // tiles from the actor's start tile to the attacked target
    static constexpr float kPounceTweenDuration = 0.12f;

    LungeAttackAction(Grid& grid, const AffixLibrary& affixes, VisualEffectSystem& visual_effects, std::mt19937& rng,
                      Vec2 direction, std::uint32_t ghost_effect_prefab_id, float ghost_effect_duration);

    ActionResult Perform(Entity actor) override;

private:
    Grid* m_grid;
    const AffixLibrary* m_affixes;
    VisualEffectSystem* m_visual_effects;
    std::mt19937* m_rng;
    Vec2 m_direction;
    std::uint32_t m_ghost_effect_prefab_id;
    float m_ghost_effect_duration;
};

} // namespace psr
