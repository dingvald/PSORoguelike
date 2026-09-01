#pragma once

#include "States/GameState.h"

namespace psr {

// Terminal state pushed by ExploringState when TurnCoordinator::Step
// returns TurnStep::PlayerDefeated (see TurnCoordinator.h's own doc
// comment) -- the last PlayerControlledComponent-tagged actor has been
// destroyed, so the turn loop underneath has nothing left to do. Publishes
// PlayerDefeatedMessage once on entry so HudLayer can show a "You Died"
// overlay; pure presentation only lives on the HudLayer side, mirroring
// CombatLogBridge's own MessageBus-only contract.
//
// Update() just holds here indefinitely -- HandleEvent() is where "press any
// button to restart" lives: the first KeyPressedEvent publishes
// RestartRequestedMessage for GameplayLayer to react to (it owns the
// registry/dungeon, so only it can actually rebuild a run), and every event
// is consumed either way so a stray keypress can't leak past this state
// while it's on top of the stack.
class GameOverState : public GameState
{
public:
    GameStateId GetId() const override { return GameStateId::GameOver; }

    void OnEnter(GameplayContext& context) override;
    StateTransition Update(GameplayContext& context, float delta_time) override;
    bool HandleEvent(Event& event, GameplayContext& context) override;

private:
    // MessageBus::Publish only enqueues -- RestartRequestedMessage isn't
    // handled until GameplayLayer's next HandleQueuedMessages() call, so two
    // KeyPressedEvents arriving before then (e.g. two keys pressed in the
    // same frame) would otherwise publish twice, and GameplayLayer would
    // process both, popping this state's own ExploringState out from under
    // it on the second one. Latched true in HandleEvent, reset in OnEnter
    // (once per death) so only the first key press this round publishes.
    bool m_restart_requested = false;
};

} // namespace psr
