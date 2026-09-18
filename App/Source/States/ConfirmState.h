#pragma once

#include "States/GameState.h"

#include <string>

namespace psr {

// What a confirmed choice should actually do once GameplayLayer's
// OnConfirmChoice pops this state back off -- only the two Quit rows
// PauseState can reach need confirmation this round (see PauseState's own
// doc comment); generalizing this to other destructive flows (drop, sell,
// delete-save) is M14.3's own future work, not this one's.
enum class ConfirmAction
{
    QuitToTitle,
    QuitToDesktop
};

// Reusable yes/no confirmation modal. Configure() must be called with the
// prompt text and pending action immediately before Push()ing this state --
// the same "configured via a non-GameState-interface method right before
// push" idiom GameStateMachine.h's own doc comment already documents for
// TargetSelectionState.
//
// Publishes ConfirmMessage(text)/ConfirmClosedMessage on enter/exit, mirroring
// every other modal state's publish-on-enter shape. HudLayer renders a Yes/No
// row list, defaulting focus to No, and resolves the player's pick into
// ConfirmChoiceMessage. Escape here pops this state directly (an implicit
// No), the same "cancel needs no message" shape CharacterScreenState's own
// Escape handling already uses -- only an actual Yes/No pick needs
// GameplayLayer to look at GetAction() and act.
class ConfirmState : public GameState
{
public:
    GameStateId GetId() const override { return GameStateId::Confirm; }

    void Configure(std::string message, ConfirmAction action);
    ConfirmAction GetAction() const { return m_action; }

    void OnEnter(GameplayContext& context) override;
    void OnExit(GameplayContext& context) override;
    StateTransition Update(GameplayContext& context, float delta_time) override;
    bool HandleEvent(Event& event, GameplayContext& context) override;

private:
    std::string m_message;
    ConfirmAction m_action = ConfirmAction::QuitToDesktop;
    bool m_close_requested = false;
};

} // namespace psr
