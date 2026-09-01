#pragma once

#include "States/GameState.h"

namespace psr {

// Pushed by ExploringState on TurnStep::AnimationsPending -- suspends the
// turn loop underneath (same pushdown mechanism TargetSelectionState/
// GameOverState already rely on) while any entity has an in-flight
// TweenComponent queue anywhere in the registry, and drives TweenSystem's
// UpdateTweens itself while it's on top (see TurnCoordinator::Step's own doc
// comment for why UpdateTweens no longer runs from there). Pops back to
// ExploringState the instant the registry is tween-free again -- by
// construction that only happens once every queued Tween's on_completion
// (e.g. AttackAction's deferred damage application) has already fired, so no
// other actor can act mid-animation and a target can't flee or die from
// something else mid-swing. No OnEnter/OnExit/HandleEvent override needed:
// GameState's defaults are already what's wanted here (no per-frame publish,
// no state to reset, input inert while animating).
class AnimationState : public GameState
{
public:
    GameStateId GetId() const override { return GameStateId::Animation; }

    StateTransition Update(GameplayContext& context, float delta_time) override;
};

} // namespace psr
