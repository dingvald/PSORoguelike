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

private:
    const AffixLibrary* m_affixes;
    bool m_close_requested = false;
};

} // namespace psr
