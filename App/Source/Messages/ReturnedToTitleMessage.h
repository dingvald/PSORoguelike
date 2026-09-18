#pragma once

namespace psr {

// Published by GameplayLayer right before it calls TransitionTo<MainMenuLayer>()
// (Quit to Title, confirmed) -- HudLayer subscribes and calls RemoveSelf(),
// since it was pushed as an overlay alongside GameplayLayer (see
// GameplayLayer::OnAttach's PushOverlay<HudLayer>()) and TransitionTo() only
// replaces the layer that calls it, not overlays stacked on top of it.
// Without this, the HUD would keep rendering and intercepting events over
// the main menu after returning to it.
struct ReturnedToTitleMessage
{
};

} // namespace psr
