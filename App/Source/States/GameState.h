#pragma once

#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"
#include "States/GameStateId.h"

#include <entt/entt.hpp>

namespace psr {

class Event;
class TurnCoordinator;

// The set of references every GameState might need -- individual states use
// whichever subset is relevant and ignore the rest, same convention as
// UnnamedRoguelike's own SystemContext. Owned by GameplayLayer, passed down
// through GameStateMachine rather than stored by any state (states are
// long-lived members constructed once; only the world they operate on can
// change between calls, e.g. across dungeon regeneration).
struct GameplayContext
{
    Registry& registry;
    Grid& grid;
    TurnCoordinator& turn_coordinator;
    entt::entity player;
};

enum class StateTransitionKind
{
    None,
    Push,
    Pop,
    Replace
};

class GameState;

// What a GameState::Update() wants GameStateMachine to do next. target is
// only meaningful for Push/Replace (null otherwise) -- a non-owning pointer
// to an already-constructed state instance (see GameStateMachine.h's own doc
// comment for why states are addressed by pointer, not a registry lookup).
struct StateTransition
{
    StateTransitionKind kind = StateTransitionKind::None;
    GameState* target = nullptr;

    static StateTransition None() { return {}; }
    static StateTransition Push(GameState& state) { return {StateTransitionKind::Push, &state}; }
    static StateTransition Pop() { return {StateTransitionKind::Pop, nullptr}; }
    static StateTransition Replace(GameState& state) { return {StateTransitionKind::Replace, &state}; }
};

// One entry on GameStateMachine's pushdown stack -- e.g. ExploringState (normal
// play) or TargetSelectionState (modal targeting cursor), ported from
// UnnamedRoguelike's own IGameState/GameStateMachine pair (renamed without the
// `I` prefix here -- per CLAUDE.md, that prefix is reserved for all-pure-
// virtual interfaces, and OnEnter/OnExit/HandleEvent below have default
// bodies, the same reasoning that keeps this project's own Layer base class
// unprefixed). Only the top of the stack receives Update()/HandleEvent()
// calls; OnEnter/OnExit bracket a state's time on top of the stack (fired by
// Push/Pop/Replace, not by construction/destruction -- state instances are
// long-lived, see GameplayContext's doc comment).
class GameState
{
public:
    virtual ~GameState() = default;

    virtual GameStateId GetId() const = 0;

    virtual void OnEnter(GameplayContext& context) { (void)context; }
    virtual void OnExit(GameplayContext& context) { (void)context; }

    virtual StateTransition Update(GameplayContext& context, float delta_time) = 0;

    // Return true to consume event (stop it reaching lower layers) -- see
    // Layer::OnEvent's own convention. Default: not handled.
    virtual bool HandleEvent(Event& event, GameplayContext& context)
    {
        (void)event;
        (void)context;
        return false;
    }
};

} // namespace psr
