#pragma once

#include "States/GameState.h"

#include <vector>

namespace psr {

// A pushdown stack of GameState instances -- ported from UnnamedRoguelike's
// own GameStateMachine. Only the top state receives Update()/HandleEvent();
// Push/Pop/Replace fire OnEnter/OnExit as a state enters/leaves the top.
//
// States are addressed by reference/pointer to an already-constructed
// instance, not looked up by GameStateId from an owned registry -- unlike the
// sibling, this machine doesn't own its states at all (GameplayLayer does, as
// long-lived members): with only two states this round (Exploring/
// TargetSelection) an owning id->instance registry would be pure overhead,
// and a state that needs to push another (e.g. ExploringState pushing
// TargetSelectionState once it's configured via a non-GameState-interface
// method) already holds a direct reference to it. GetId() still exists on
// GameState for introspection/debugging even though this machine doesn't use
// it to dispatch.
class GameStateMachine
{
public:
    void Push(GameState& state, GameplayContext& context);
    void Pop(GameplayContext& context);
    void Replace(GameState& state, GameplayContext& context);

    GameState* Top() const { return m_stack.empty() ? nullptr : m_stack.back(); }
    bool IsEmpty() const { return m_stack.empty(); }

    // Runs Top()->Update(), then applies whatever StateTransition it returns.
    // A no-op if the stack is empty.
    void Update(GameplayContext& context, float delta_time);

    // Forwards to Top()->HandleEvent(). Returns false (unhandled) if the
    // stack is empty.
    bool HandleEvent(Event& event, GameplayContext& context);

private:
    std::vector<GameState*> m_stack;
};

} // namespace psr
