#pragma once

namespace psr {

// Published by StorageState::OnExit; HudLayer hides the Storage panel and
// clears its per-row click listeners in response.
struct StorageClosedMessage
{
};

} // namespace psr
