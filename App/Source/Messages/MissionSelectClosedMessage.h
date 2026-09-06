#pragma once

namespace psr {

// Published by MissionSelectState::OnExit; HudLayer hides the Mission
// Select panel and clears its per-row click listeners in response.
struct MissionSelectClosedMessage
{
};

} // namespace psr
