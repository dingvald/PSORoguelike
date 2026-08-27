#pragma once

#include "Engine/Layer.h"

#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace Rml {
class ElementDocument;
} // namespace Rml

namespace psr {

class RmlClickListener;
struct PlayerStatusMessage;
struct HotbarStateMessage;
struct CombatLogEntryMessage;

// Player HUD overlay: HP/TP bars, the 10-slot Technique/Photon Art/Item
// hotbar, and a scrolling event log. Pushed as an overlay from
// GameplayLayer::OnAttach (PushOverlay<HudLayer>()), with its own
// hud.rml/hud.rcss document.
//
// Holds no reference to Registry, any entt::entity, or any content library --
// pure presentation, driven entirely by messages (PlayerStatusMessage/
// HotbarStateMessage/CombatLogEntryMessage). It caches the latest values it's
// been sent and renders from that cache; it never retains the ECS/registry
// data those messages were built from. Slot clicks are published back onto
// the bus as HotbarSlotActivatedMessage -- this layer never holds a
// reference back to GameplayLayer either, per Layer.h's "layers never hold
// references to each other."
class HudLayer : public Layer
{
public:
    HudLayer();
    ~HudLayer() override;

    HudLayer(const HudLayer&) = delete;
    HudLayer& operator=(const HudLayer&) = delete;
    HudLayer(HudLayer&&) = delete;
    HudLayer& operator=(HudLayer&&) = delete;

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(float delta_time) override;

private:
    void LoadDocument();
    void WireHotbarSlots();

    void OnPlayerStatus(const PlayerStatusMessage& message);
    void OnHotbarState(const HotbarStateMessage& message);
    void OnLogEntry(const CombatLogEntryMessage& message);

    Rml::ElementDocument* m_document = nullptr;
    std::vector<std::unique_ptr<RmlClickListener>> m_hotbar_listeners;

    static constexpr std::size_t kMaxLogLines = 50;
    std::deque<std::string> m_log_lines; // already-formatted text from CombatLogEntryMessage
};

} // namespace psr
