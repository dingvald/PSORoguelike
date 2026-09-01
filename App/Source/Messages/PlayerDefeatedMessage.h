#pragma once

namespace psr {

// Published once by GameOverState::OnEnter when TurnCoordinator::Step
// reports TurnStep::PlayerDefeated (see TurnCoordinator.h) -- HudLayer
// reacts by showing a "You Died" overlay. Plain data, same no-Registry/no-
// entt::entity contract as HudReadyMessage/PlayerStatusMessage.
struct PlayerDefeatedMessage
{
};

} // namespace psr
