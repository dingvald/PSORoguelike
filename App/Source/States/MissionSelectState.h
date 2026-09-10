#pragma once

#include "States/GameState.h"

namespace psr {

class DungeonLibrary;
class AreaLibrary;
struct RunProgress;

// Modal Mission Select screen, pushed by GameplayLayer's Space-key
// interaction handling when the player is standing on the hub's teleporter
// entity (see Hub/HubInteraction.h), popped by this state itself on
// Space-with-nothing-focused-changing/Escape. Mirrors CharacterScreenState/
// ActionPaletteState's exact push/publish-on-enter/close-on-Escape shape
// -- nothing here is a turn-costing IAction (entering a mission is a scene
// swap, not a queued action), so no RequestClose() is needed.
class MissionSelectState : public GameState
{
public:
    MissionSelectState(const DungeonLibrary& dungeons, const RunProgress& progress, const AreaLibrary& areas);

    GameStateId GetId() const override { return GameStateId::MissionSelect; }

    void OnEnter(GameplayContext& context) override;
    void OnExit(GameplayContext& context) override;
    StateTransition Update(GameplayContext& context, float delta_time) override;
    bool HandleEvent(Event& event, GameplayContext& context) override;

private:
    const DungeonLibrary* m_dungeons;
    const RunProgress* m_progress;
    const AreaLibrary* m_areas;
    bool m_close_requested = false;
};

} // namespace psr
