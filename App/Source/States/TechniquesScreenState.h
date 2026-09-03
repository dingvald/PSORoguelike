#pragma once

#include "States/GameState.h"

namespace psr {

class TechniqueLibrary;
class PhotonArtLibrary;

// Modal Techniques/Photon Arts screen (learned Techniques + the equipped
// weapon's granted Photon Arts, each assignable to the hotbar) -- pushed by
// GameplayLayer::OnEvent when 'T' is pressed while ExploringState is on top,
// popped by this state itself on 'T'/Escape. Mirrors CharacterScreenState
// exactly, minus RequestClose(): unlike the Character screen's Use/Drop
// actions, nothing on this screen is a real IAction -- hotbar-assignment is
// free/instant (see Items/Hotbar.h's AssignAbilityToHotbarSlot), so there's
// never a reason for this screen to ask to be closed out from under itself.
class TechniquesScreenState : public GameState
{
public:
    TechniquesScreenState(const TechniqueLibrary& techniques, const PhotonArtLibrary& photon_arts);

    GameStateId GetId() const override { return GameStateId::TechniquesScreen; }

    void OnEnter(GameplayContext& context) override;
    void OnExit(GameplayContext& context) override;
    StateTransition Update(GameplayContext& context, float delta_time) override;
    bool HandleEvent(Event& event, GameplayContext& context) override;

private:
    const TechniqueLibrary* m_techniques;
    const PhotonArtLibrary* m_photon_arts;
    bool m_close_requested = false;
};

} // namespace psr
