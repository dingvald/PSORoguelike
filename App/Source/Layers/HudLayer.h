#pragma once

#include "Engine/Layer.h"
#include "Messages/CharacterScreenMessage.h"

#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Rml {
class Element;
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

    // Intercepts numpad/space/escape navigation for the Character screen
    // while it's open (Application::OnEvent gives overlays first crack at
    // every event, before GameplayLayer -- see its own doc comment). Pure
    // local UI state (focus, open menu, or -- after "Assign to Hotbar" --
    // awaiting a 0-9 slot keypress); only publishes a message back onto the
    // bus when a choice actually mutates game state (Equip/Use/Drop/Remove/
    // AssignToHotbar). Does nothing while the screen is closed or Escape is
    // pressed with no menu open and no slot pick awaited, so
    // CharacterScreenState::HandleEvent still closes the whole screen in
    // that case.
    void OnEvent(Event& event) override;

private:
    // Which of the Character screen's three equal panels currently has
    // keyboard focus. Stats has no rows -- navigating onto it just parks
    // focus there with nothing selectable, per design.
    enum class CharacterScreenPanel
    {
        Stats,
        Equipment,
        Inventory
    };

    // One row of the open context menu -- action is what ChooseHighlightedMenuOption
    // dispatches on, label is the literal RML text.
    struct ContextMenuOption
    {
        enum class Action
        {
            Equip,
            Remove,
            Use,
            Drop,
            AssignToHotbar
        };

        Action action;
        std::string label;
    };

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

    // Shows the Character screen panel and rebuilds its Stats/Equipment/
    // Inventory contents (SetInnerRML once per container, then
    // QuerySelectorAll to recover per-row elements and attach one
    // RmlClickListener each -- same recipe WireHotbarSlots uses for the
    // fixed-count hotbar, just rebuilt every call since the inventory's
    // length isn't fixed). Resets focus to {Stats, 0} and closes any open
    // menu on a fresh open (m_character_screen_cache was empty); on a
    // republish after an in-screen action, re-clamps the existing focus into
    // the new bounds instead.
    void OnCharacterScreenState(const CharacterScreenMessage& message);
    void OnCharacterScreenClosed(const CharacterScreenClosedMessage& message);

    // Rebuilds #floating-text-layer every call (published every frame by
    // GameplayLayer) -- one positioned, non-interactive span per active
    // FloatingTextSystem instance, left/top/color set inline since they're
    // per-instance, not shared CSS.
    void OnFloatingTextState(const FloatingTextStateMessage& message);

    void AppendLogLine(const std::string& text);

    // Character-screen navigation/context-menu helpers -- see OnEvent's doc
    // comment. Split out of OnEvent itself so both the keyboard path and the
    // RmlClickListener callbacks on rows/menu options can share them.
    void RenderStatsPanel(const CharacterScreenMessage::StatsSummary& stats);
    int CharacterScreenRowCount(CharacterScreenPanel panel) const;
    void MovePanelFocus(int direction);
    void MoveRowFocus(int direction);
    void ActivateFocusedRow();
    void OpenContextMenu(CharacterScreenPanel panel, int index);
    void CloseContextMenu();
    void MoveMenuHighlight(int direction);
    void ChooseHighlightedMenuOption();
    void JumpToMatchingInventoryItem(EquipmentSlot slot);

    // "Assign to Hotbar" sub-state: menu closes, hint text changes, and
    // OnEvent waits for a 0-9 keypress (or Escape to cancel) instead of the
    // usual numpad navigation -- see OnEvent's doc comment.
    void BeginAwaitingHotbarSlot(int inventory_index);
    void CancelAwaitingHotbarSlot();
    void SetCharacterScreenHint(const char* text, bool awaiting);
    std::vector<ContextMenuOption> BuildMenuOptions(CharacterScreenPanel panel, int index) const;
    void RenderContextMenu();
    void UpdateMenuHighlightClasses();
    void RenderFocusHighlights();
    void RenderRowFocus(const char* container_id, const char* row_class, CharacterScreenPanel panel);
    Rml::Element* CharacterScreenRowElement(CharacterScreenPanel panel, int index);

    Rml::ElementDocument* m_document = nullptr;
    std::vector<std::unique_ptr<RmlClickListener>> m_hotbar_listeners;

    // Rebuilt on every OnCharacterScreenState call (unlike m_hotbar_listeners'
    // fixed 10 slots) -- the inventory's row count changes as items are
    // picked up/equipped/unequipped.
    std::vector<std::unique_ptr<RmlClickListener>> m_character_screen_listeners;

    // The latest CharacterScreenMessage, kept around so OnEvent's keyboard
    // handling knows row counts/item kinds without touching ECS (this layer
    // otherwise holds no Registry reference, see class doc comment). Empty
    // while the screen is closed -- OnEvent uses that to decide whether to
    // intercept numpad/space/escape at all. Set in OnCharacterScreenState,
    // cleared in OnCharacterScreenClosed.
    std::optional<CharacterScreenMessage> m_character_screen_cache;

    CharacterScreenPanel m_focused_panel = CharacterScreenPanel::Stats;
    int m_focused_row = 0;

    bool m_menu_open = false;
    CharacterScreenPanel m_menu_panel = CharacterScreenPanel::Equipment;
    int m_menu_index = 0;
    int m_menu_highlight = 0;
    std::vector<ContextMenuOption> m_menu_options;
    std::vector<std::unique_ptr<RmlClickListener>> m_context_menu_listeners;

    // Set by BeginAwaitingHotbarSlot ("Assign to Hotbar" chosen, waiting on a
    // 0-9 keypress); mutually exclusive with m_menu_open.
    bool m_awaiting_hotbar_slot = false;
    int m_awaiting_hotbar_inventory_index = -1;

    static constexpr std::size_t kMaxLogLines = 50;
    std::deque<std::string> m_log_lines; // already-formatted text from CombatLogEntryMessage/LootDropMessage
};

} // namespace psr
