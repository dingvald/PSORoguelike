#pragma once

namespace psr {

// Published by CharacterScreenState::OnExit; HudLayer hides the Character
// screen panel and clears its per-row click listeners in response.
struct CharacterScreenClosedMessage
{
};

} // namespace psr
