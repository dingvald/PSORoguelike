#pragma once

namespace psr {

// Published by GameOverState::HandleEvent on the first key press while it's
// on top of the state stack ("press any button to restart"). GameplayLayer
// subscribes and responds by rebuilding the whole run (see
// GameplayLayer::LoadNewGame) and popping GameOverState back off the state
// stack. Plain data, same no-Registry/no-entt::entity contract as
// HudReadyMessage/PlayerDefeatedMessage.
struct RestartRequestedMessage
{
};

} // namespace psr
