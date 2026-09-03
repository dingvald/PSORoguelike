#pragma once

#include "States/GameState.h"

namespace psr {

class TargetSelectionState;
class GameOverState;
class AnimationState;

// Normal play: forwards key-down events into TurnCoordinator::PressKey
// (key-up is handled globally by GameplayLayer::OnEvent, not gated to this
// state -- see its own doc comment) and drives its Step() loop every
// Update() -- the same behavior GameplayLayer::OnEvent/OnUpdate performed
// directly before this state machine existed, now hosted as the base/bottom
// state on GameStateMachine's stack so a modal state (TargetSelectionState)
// can suspend it the same way UnnamedRoguelike's own ExploringState is
// suspended by its TargetSelection/Animating/Inventory states.
// TurnStep::PlayerDefeated suspends it the same way, pushing GameOverState
// instead -- and TurnStep::AnimationsPending pushes AnimationState, once a
// resolved Move/Attack has queued a Tween.
class ExploringState : public GameState
{
public:
    ExploringState(TargetSelectionState& target_selection, GameOverState& game_over, AnimationState& animation);

    GameStateId GetId() const override { return GameStateId::Exploring; }

    StateTransition Update(GameplayContext& context, float delta_time) override;
    bool HandleEvent(Event& event, GameplayContext& context) override;

private:
    TargetSelectionState* m_target_selection;
    GameOverState* m_game_over;
    AnimationState* m_animation;
};

} // namespace psr
