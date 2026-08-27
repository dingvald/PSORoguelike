#pragma once

namespace psr {

// Published by HudLayer once, right after it subscribes to
// PlayerStatusMessage/HotbarStateMessage/CombatLogEntryMessage in its own
// OnAttach(). GameplayLayer subscribes to this and responds by re-publishing
// current state.
//
// This handshake exists because PushOverlay<HudLayer>() is deferred a full
// frame (see Application::Run()'s pending-layer-stack-changes step): a
// message GameplayLayer publishes during its own OnAttach() would reach
// HudLayer's subscription too early -- HudLayer doesn't exist yet -- and
// MessageBus doesn't replay past messages for late subscribers. Publishing
// HudReadyMessage from HudLayer's OnAttach() instead guarantees the state
// re-publish happens after HudLayer has subscribed: GameplayLayer::OnUpdate()
// (which drains this message) runs before HudLayer::OnUpdate() in the same
// frame, since regular layers update before overlays.
struct HudReadyMessage
{
};

} // namespace psr
