#pragma once

#include "States/GameState.h"

namespace psr {

class TargetSelectionState;

// Normal play: forwards key events into TurnCoordinator::PressKey/
// ReleaseKey and drives its Step() loop every Update() -- the same behavior
// GameplayLayer::OnEvent/OnUpdate performed directly before this state
// machine existed, now hosted as the base/bottom state on GameStateMachine's
// stack so a modal state (TargetSelectionState) can suspend it the same way
// UnnamedRoguelike's own ExploringState is suspended by its TargetSelection/
// Animating/Inventory states.
class ExploringState : public GameState
{
public:
    explicit ExploringState(TargetSelectionState& target_selection);

    GameStateId GetId() const override { return GameStateId::Exploring; }

    StateTransition Update(GameplayContext& context, float delta_time) override;
    bool HandleEvent(Event& event, GameplayContext& context) override;

private:
    TargetSelectionState* m_target_selection;
};

} // namespace psr
