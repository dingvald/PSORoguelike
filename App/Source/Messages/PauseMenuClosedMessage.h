#pragma once

namespace psr {

// Published by PauseState::OnExit; HudLayer hides the pause overlay (and any
// open Options/Help placeholder within it) in response.
struct PauseMenuClosedMessage
{
};

} // namespace psr
