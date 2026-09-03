#pragma once

namespace psr {

// Identifies a concrete IGameState for introspection/debugging (e.g. logging
// which state is active) -- not used to look states up, since
// GameStateMachine::Push/Replace take the state instance directly (see
// GameStateMachine.h). Grown as new states land (Inventory/Equipment/menu
// states are the anticipated next additions, per the M7.2 plan's rationale
// for porting this framework now).
enum class GameStateId
{
    Exploring,
    TargetSelection,
    GameOver,
    CharacterScreen,
    Animation,
    TechniquesScreen,
};

} // namespace psr
