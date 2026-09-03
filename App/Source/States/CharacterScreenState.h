#pragma once

#include "States/GameState.h"

namespace psr {

class AffixLibrary;

// Modal Character screen (Inventory + Equipment as simple lists), pushed by
// GameplayLayer::OnEvent when 'C' is pressed while ExploringState is on top,
// popped by this state itself on 'C'/Escape. Pushing it already suspends
// TurnCoordinator::Step() the same way TargetSelectionState does (only the
// top of GameStateMachine's stack updates), so equip/unequip (see Equip.h)
// are free/instant mutations, not IActions -- nothing else can act while
// this screen is open regardless, and routing them through
// TurnCoordinator::SetPendingAction would force the screen to close after
// every single click.
//
// No per-frame logic (unlike TargetSelectionState's cursor), so Update()
// only ever turns a HandleEvent-set close flag into StateTransition::Pop() --
// same split TargetSelectionState uses, required because
// GameState::HandleEvent returns bool, not a StateTransition.
class CharacterScreenState : public GameState
{
public:
    explicit CharacterScreenState(const AffixLibrary& affixes);

    GameStateId GetId() const override { return GameStateId::CharacterScreen; }

    void OnEnter(GameplayContext& context) override;
    void OnExit(GameplayContext& context) override;
    StateTransition Update(GameplayContext& context, float delta_time) override;
    bool HandleEvent(Event& event, GameplayContext& context) override;

    // Called by GameplayLayer after submitting a Use/Drop action chosen from
    // the Character screen's Inventory context menu -- those are real
    // energy-costing IActions (see UseItemAction.h/DropAction.h), and
    // SetPendingAction only resolves once ExploringState is back on top of
    // the state stack, so the screen must close for the turn to actually
    // happen. Same close-next-Update() plumbing HandleEvent's Escape/C uses.
    void RequestClose() { m_close_requested = true; }

private:
    const AffixLibrary* m_affixes;
    bool m_close_requested = false;
};

} // namespace psr
