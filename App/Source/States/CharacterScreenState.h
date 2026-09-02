#pragma once

#include "States/GameState.h"

namespace psr {

class AffixLibrary;
struct LevelingConfig;
struct CharacterScreenFocus;

// Modal Character screen (Inventory + Equipment lists, plus a stats panel),
// pushed by GameplayLayer::OnEvent when 'C' is pressed while ExploringState
// is on top, popped by this state itself on 'C'/Escape. Pushing it already
// suspends TurnCoordinator::Step() the same way TargetSelectionState does
// (only the top of GameStateMachine's stack updates), so equip/unequip (see
// Equip.h) are free/instant mutations, not IActions -- nothing else can act
// while this screen is open regardless, and routing them through
// TurnCoordinator::SetPendingAction would force the screen to close after
// every single click.
//
// Fully keyboard-navigable in addition to HudLayer's mouse click/hover
// handling: Up/W/Numpad-8 and Down/S/Numpad-2 move a focus cursor over one
// flat, wrapping list (the 5 equipment slots, then every inventory item, in
// that order), Space activates whatever's focused (equip an inventory item,
// unequip an equipment slot) by calling EquipItem/UnequipSlot directly --
// not by publishing the same message a mouse click does, since
// MessageBus::Publish only enqueues (handler invocation happens on the
// *next* HandleQueuedMessages() call), which would show a stale focus/stats
// snapshot for a frame; doing the mutation and rebuilding the message here,
// synchronously, in the same HandleEvent call avoids that.
//
// No per-frame logic otherwise (unlike TargetSelectionState's cursor), so
// Update() only ever turns a HandleEvent-set close flag into
// StateTransition::Pop() -- same split TargetSelectionState uses, required
// because GameState::HandleEvent returns bool, not a StateTransition.
class CharacterScreenState : public GameState
{
public:
    CharacterScreenState(const AffixLibrary& affixes, const LevelingConfig& leveling);

    GameStateId GetId() const override { return GameStateId::CharacterScreen; }

    void OnEnter(GameplayContext& context) override;
    void OnExit(GameplayContext& context) override;
    StateTransition Update(GameplayContext& context, float delta_time) override;
    bool HandleEvent(Event& event, GameplayContext& context) override;

private:
    int RowCount(GameplayContext& context) const;
    CharacterScreenFocus CurrentFocus() const;
    void MoveFocus(GameplayContext& context, int delta);
    void ActivateFocused(GameplayContext& context);
    void PublishSnapshot(GameplayContext& context);

    const AffixLibrary* m_affixes;
    const LevelingConfig* m_leveling;
    bool m_close_requested = false;

    // 0-4 are the equipment slots (Weapon/Head/Torso/Hands/Legs, matching
    // HudLayer::OnCharacterScreenState's build order), 5.. are inventory
    // items -- see CurrentFocus/RowCount.
    int m_focused_index = 0;
};

} // namespace psr
