#pragma once

namespace psr {

// Published by TechniquesScreenState::OnExit; HudLayer hides the Techniques
// screen panel and clears its per-row click listeners in response -- mirrors
// CharacterScreenClosedMessage exactly.
struct TechniquesScreenClosedMessage
{
};

} // namespace psr
