#pragma once

#include "Components/HotbarComponent.h"
#include "Engine/Layer.h"
#include "Messages/CharacterScreenMessage.h"
#include "Messages/TechniquesScreenMessage.h"

#include <cstddef>
#include <cstdint>
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
struct TechniquesScreenClosedMessage;
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

    // Intercepts numpad/space/escape navigation for the Character screen and
    // the Techniques/Photon Arts screen while either is open
    // (Application::OnEvent gives overlays first crack at every event,
    // before GameplayLayer -- see its own doc comment). Pure local UI state
    // (focus, open menu, or -- after "Assign to Hotbar" -- awaiting a 0-9
    // slot keypress); only publishes a message back onto the bus when a
    // choice actually mutates game state (Equip/Use/Drop/Remove/
    // AssignToHotbar). Does nothing while both screens are closed, or Escape
    // is pressed with no menu open and no slot pick awaited, so
    // CharacterScreenState/TechniquesScreenState's own HandleEvent still
    // closes the screen in that case.
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

    // Which of the Techniques/Photon Arts screen's two panels currently has
    // keyboard focus.
    enum class TechniquesScreenPanel
    {
        Techniques,
        PhotonArts
    };

    // Which screen the pending "awaiting a 0-9 hotbar slot" sub-state (see
    // BeginAwaitingHotbarSlot/BeginAwaitingAbilityHotbarSlot) started from --
    // determines which message OnEvent publishes once a slot key is pressed,
    // and which screen's hint element CancelAwaitingHotbarSlot restores.
    // Replaces a plain bool now that two screens share this sub-state.
    enum class HotbarAssignSource
    {
        None,
        CharacterScreenItem,
        TechniquesScreenAbility
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

    // Same shape as OnCharacterScreenState/OnCharacterScreenClosed, for the
    // Techniques/Photon Arts screen -- two panels, no Stats-equivalent, no
    // context menu (a row's only action is "assign to hotbar", so Space goes
    // straight into the awaiting-slot sub-state via ActivateFocusedTechRow).
    void OnTechniquesScreenState(const TechniquesScreenMessage& message);
    void OnTechniquesScreenClosed(const TechniquesScreenClosedMessage& message);

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

    // Techniques-screen navigation helpers -- same split-out-of-OnEvent
    // reasoning as the Character-screen helpers above, just without a
    // context-menu layer (a row has exactly one action).
    int TechniquesScreenRowCount(TechniquesScreenPanel panel) const;
    void MoveTechPanelFocus(int direction);
    void MoveTechRowFocus(int direction);
    void ActivateFocusedTechRow();
    void RenderTechniquesFocusHighlights();
    void RenderTechRowFocus(const char* container_id, const char* row_class, TechniquesScreenPanel panel);

    // "Assign to Hotbar" sub-state: menu closes, hint text changes, and
    // OnEvent waits for a 0-9 keypress (or Escape to cancel) instead of the
    // usual numpad navigation -- see OnEvent's doc comment. Shared between
    // the Character screen's inventory-item flow and the Techniques screen's
    // ability flow (see HotbarAssignSource).
    void BeginAwaitingHotbarSlot(int inventory_index);
    void BeginAwaitingAbilityHotbarSlot(HotbarSlotType type, std::uint32_t id);
    void CancelAwaitingHotbarSlot();
    void SetCharacterScreenHint(const char* text, bool awaiting);
    void SetTechniquesScreenHint(const char* text, bool awaiting);
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

    // Rebuilt on every OnTechniquesScreenState call -- same reasoning as
    // m_character_screen_listeners (row count isn't fixed).
    std::vector<std::unique_ptr<RmlClickListener>> m_techniques_screen_listeners;

    // Same "cache drives OnEvent interception, empty means closed" contract
    // as m_character_screen_cache. Set in OnTechniquesScreenState, cleared in
    // OnTechniquesScreenClosed.
    std::optional<TechniquesScreenMessage> m_techniques_screen_cache;

    TechniquesScreenPanel m_tech_focused_panel = TechniquesScreenPanel::Techniques;
    int m_tech_focused_row = 0;

    // Set by BeginAwaitingHotbarSlot/BeginAwaitingAbilityHotbarSlot ("Assign
    // to Hotbar" chosen, waiting on a 0-9 keypress); mutually exclusive with
    // m_menu_open. Which of the two payload fields below is meaningful
    // depends on the source.
    HotbarAssignSource m_awaiting_hotbar_assign_source = HotbarAssignSource::None;
    int m_awaiting_hotbar_inventory_index = -1;                          // CharacterScreenItem
    HotbarSlotType m_awaiting_hotbar_ability_type = HotbarSlotType::Empty; // TechniquesScreenAbility
    std::uint32_t m_awaiting_hotbar_ability_id = 0;                       // TechniquesScreenAbility

    static constexpr std::size_t kMaxLogLines = 50;
    std::deque<std::string> m_log_lines; // already-formatted text from CombatLogEntryMessage/LootDropMessage
};

} // namespace psr
