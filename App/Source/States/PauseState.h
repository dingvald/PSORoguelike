#pragma once

#include "States/GameState.h"

namespace psr {

// Modal pause menu -- pushed by GameplayLayer::OnEvent when Escape is
// pressed while ExploringState is on top and there's no tab-target to clear
// instead (M14.2's escape hierarchy: a modal closes itself on Escape, a
// tab-target lock is cleared directly in GameplayLayer::OnEvent, and this is
// what's left once neither applies). Mirrors MissionSelectState's exact
// shape -- publish on enter, close on Escape, no per-frame logic of its own.
//
// The row set (Resume/Options/Help/Quit to Title/Quit to Desktop) is fixed
// engine UI rather than authored content, so PauseMenuMessage carries no
// entries -- the rows are baked directly into hud.rml, and HudLayer resolves
// an activated row into PauseMenuActionMessage for GameplayLayer's
// OnPauseMenuAction to act on (Resume pops this state directly; the two Quit
// rows push ConfirmState on top of it first, since both discard the current
// run with no save yet, M11.2).
class PauseState : public GameState
{
public:
    GameStateId GetId() const override { return GameStateId::Pause; }

    void OnEnter(GameplayContext& context) override;
    void OnExit(GameplayContext& context) override;
    StateTransition Update(GameplayContext& context, float delta_time) override;
    bool HandleEvent(Event& event, GameplayContext& context) override;

private:
    bool m_close_requested = false;
};

} // namespace psr
