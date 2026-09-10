#pragma once

namespace psr {

// Published by ActionPaletteState::OnExit; HudLayer hides the Techniques
// screen panel and clears its per-row click listeners in response -- mirrors
// CharacterScreenClosedMessage exactly.
struct ActionPaletteClosedMessage
{
};

} // namespace psr
