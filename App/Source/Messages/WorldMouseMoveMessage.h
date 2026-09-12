#pragma once

namespace psr {

// Published by HudLayer on every "mousemove" over hud.rml's body -- see
// WorldMouseDownMessage.h's own doc comment for why this is a bare forwarded
// report rather than something HudLayer resolves itself. Drives both
// hover-to-inspect (GameplayLayer::OnWorldMouseMove) and, indirectly, the
// tooltip HudLayer itself renders in response to the resulting
// WorldTileHoverMessage.
struct WorldMouseMoveMessage
{
    float screen_x = 0.0f;
    float screen_y = 0.0f;
};

} // namespace psr
