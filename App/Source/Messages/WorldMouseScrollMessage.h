#pragma once

namespace psr {

// Published by HudLayer on every "mousescroll" over hud.rml's body -- see
// WorldMouseDownMessage.h's own doc comment for why this is a bare forwarded
// report rather than something HudLayer resolves itself. wheel_delta_y
// matches RmlUi's own "mousescroll" event parameter: positive scrolls down/
// away, negative scrolls up/toward (mouse-wheel convention) -- see
// GameplayLayer::OnWorldMouseScroll, which maps it onto the same
// Camera::SetZoom the KP_PLUS/KP_MINUS binding already drives.
struct WorldMouseScrollMessage
{
    float wheel_delta_y = 0.0f;
};

} // namespace psr
