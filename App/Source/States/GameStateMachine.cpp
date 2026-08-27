#include "States/GameStateMachine.h"

namespace psr {

void GameStateMachine::Push(GameState& state, GameplayContext& context)
{
    m_stack.push_back(&state);
    state.OnEnter(context);
}

void GameStateMachine::Pop(GameplayContext& context)
{
    if (m_stack.empty())
        return;
    m_stack.back()->OnExit(context);
    m_stack.pop_back();
}

void GameStateMachine::Replace(GameState& state, GameplayContext& context)
{
    Pop(context);
    Push(state, context);
}

void GameStateMachine::Update(GameplayContext& context, float delta_time)
{
    if (m_stack.empty())
        return;

    const StateTransition transition = m_stack.back()->Update(context, delta_time);
    switch (transition.kind)
    {
    case StateTransitionKind::Push:
        Push(*transition.target, context);
        break;
    case StateTransitionKind::Pop:
        Pop(context);
        break;
    case StateTransitionKind::Replace:
        Replace(*transition.target, context);
        break;
    case StateTransitionKind::None:
        break;
    }
}

bool GameStateMachine::HandleEvent(Event& event, GameplayContext& context)
{
    if (m_stack.empty())
        return false;
    return m_stack.back()->HandleEvent(event, context);
}

} // namespace psr
