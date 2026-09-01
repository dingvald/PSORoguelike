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
struct StatusEffectsMessage;
struct PlayerDefeatedMessage;
struct GameRestartedMessage;
struct LootDropMessage;
struct MesetaChangedMessage;
struct CharacterScreenMessage;
struct CharacterScreenClosedMessage;
struct FloatingTextStateMessage;

// Player HUD overlay: HP/TP bars, the 10-slot Technique/Photon Art/Item
// hotbar, a status-effect icon+duration row, and a scrolling event log.
// Pushed as an overlay from GameplayLayer::OnAttach (PushOverlay<HudLayer>()),
// with its own hud.rml/hud.rcss document.
//
// Holds no reference to Registry, any entt::entity, or any content library --
// pure presentation, driven entirely by messages (PlayerStatusMessage/
// HotbarStateMessage/CombatLogEntryMessage/StatusEffectsMessage). It caches
// the latest values it's
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
    void OnStatusEffects(const StatusEffectsMessage& message);
    void OnPlayerDefeated(const PlayerDefeatedMessage& message);
    void OnGameRestarted(const GameRestartedMessage& message);
    void OnLootDrop(const LootDropMessage& message);
    void OnMesetaChanged(const MesetaChangedMessage& message);

    // Shows the Character screen panel and rebuilds its Equipment/Inventory
    // row lists (SetInnerRML once per container, then QuerySelectorAll to
    // recover per-row elements and attach one RmlClickListener each --
    // same recipe WireHotbarSlots uses for the fixed-count hotbar, just
    // rebuilt every call since the inventory's length isn't fixed).
    void OnCharacterScreenState(const CharacterScreenMessage& message);
    void OnCharacterScreenClosed(const CharacterScreenClosedMessage& message);

    // Rebuilds #floating-text-layer every call (published every frame by
    // GameplayLayer) -- one positioned, non-interactive span per active
    // FloatingTextSystem instance, left/top/color set inline since they're
    // per-instance, not shared CSS.
    void OnFloatingTextState(const FloatingTextStateMessage& message);

    void AppendLogLine(const std::string& text);

    Rml::ElementDocument* m_document = nullptr;
    std::vector<std::unique_ptr<RmlClickListener>> m_hotbar_listeners;

    // Rebuilt on every OnCharacterScreenState call (unlike m_hotbar_listeners'
    // fixed 10 slots) -- the inventory's row count changes as items are
    // picked up/equipped/unequipped.
    std::vector<std::unique_ptr<RmlClickListener>> m_character_screen_listeners;

    static constexpr std::size_t kMaxLogLines = 50;
    std::deque<std::string> m_log_lines; // already-formatted text from CombatLogEntryMessage/LootDropMessage
};

} // namespace psr
