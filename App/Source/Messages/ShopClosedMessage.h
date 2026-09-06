#pragma once

namespace psr {

// Published by ShopState::OnExit; HudLayer hides the Shop panel and clears
// its per-row click listeners in response.
struct ShopClosedMessage
{
};

} // namespace psr
