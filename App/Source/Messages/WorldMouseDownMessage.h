#pragma once

namespace psr {

// Published by HudLayer when its RmlEventListener catches a "mousedown" on
// hud.rml's body -- HudLayer holds no gameplay state of its own, so this is
// purely a forwarded raw screen-space report for GameplayLayer to resolve
// into a tile via PixelToTile (see TileVertexMath.h) and act on (click-to-
// move/click-to-target, depending on the active GameState -- see
// GameplayLayer::OnWorldMouseDown). button follows RmlUi's own convention:
// 0 = left, 1 = right, 2 = middle.
struct WorldMouseDownMessage
{
    float screen_x = 0.0f;
    float screen_y = 0.0f;
    int button = 0;
};

} // namespace psr
