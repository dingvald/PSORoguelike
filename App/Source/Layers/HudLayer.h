#pragma once

#include "Components/HotbarComponent.h"
#include "Engine/Layer.h"
#include "Messages/CharacterScreenMessage.h"
#include "Messages/CharacterScreenStatPreviewMessage.h"
#include "Messages/MissionSelectMessage.h"
#include "Messages/ShopMessage.h"
#include "Messages/StorageMessage.h"
#include "Messages/ActionPaletteMessage.h"

#include <array>
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
class RmlEventListener;
class RmlHoverListener;
class RmlScrollListener;
struct PlayerStatusMessage;
struct HotbarStateMessage;
struct CombatLogEntryMessage;
struct StatusEffectsMessage;
struct PlayerDefeatedMessage;
struct GameRestartedMessage;
struct LootDropMessage;
struct CharacterScreenClosedMessage;
struct ActionPaletteClosedMessage;
struct FloatingTextStateMessage;
struct TargetStateMessage;
struct HubInteractionPromptMessage;
struct TeleporterPromptMessage;
struct MissionCompletedMessage;
struct MissionSelectClosedMessage;
struct ShopClosedMessage;
struct StorageClosedMessage;
struct WorldTileHoverMessage;

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
    // CharacterScreenState/ActionPaletteState's own HandleEvent still
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

    // Which of the Action Palette screen's four panels currently has
    // keyboard focus.
    enum class ActionPalettePanel
    {
        NormalAttack,
        SpecialAttack,
        Techniques,
        PhotonArts
    };

    // Which of the Shop screen's two panels currently has keyboard focus.
    enum class ShopScreenPanel
    {
        Stock,
        Sellable
    };

    // Which of the Storage screen's two panels currently has keyboard focus.
    enum class StorageScreenPanel
    {
        Inventory,
        Storage
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
        ActionPaletteAbility
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
            AssignToHotbar,
            Feed
        };

        Action action;
        std::string label;

        // True when the action is valid for this row but not currently
        // selectable (e.g. Feed while the mag's feed cooldown is active) --
        // still rendered, greyed out via RML's own "disabled" class, but
        // ChooseHighlightedMenuOption no-ops on it.
        bool disabled = false;
    };

    // One mag stat bar's feed-animation state, tracked in raw sub-units
    // (level * progress_to_next + progress -- see CharacterScreenMessage::
    // MagStatBar) so a multi-level gain from one feed animates as a single
    // continuous fill that visibly wraps to 0% at each level boundary
    // instead of jumping straight to the final bar position.
    // displayed_level/level_up_flash track what's actually been rendered so
    // UpdateMagPanelAnimations can detect a level-up crossing as
    // displayed_subunits sweeps past it, even when target_subunits already
    // reflects several levels of gain.
    struct MagStatAnimation
    {
        float displayed_subunits = 0.0f;
        int target_subunits = 0;
        int displayed_level = 0;
        float level_up_flash = 0.0f;
    };

    // Child elements of #character-screen-mag-panel, captured once when its
    // row markup is first built (see RenderMagPanel) so every later refresh
    // -- especially the per-frame ones from UpdateMagPanelAnimations -- can
    // update text/width directly instead of rebuilding markup (which would
    // destroy and recreate .mag-stat-bar-fill every call, leaving no
    // previous-frame width for the animation to read as its starting point).
    // Index order matches MagSummary's pow/def/dex/mind fields.
    struct MagPanelElements
    {
        Rml::Element* title = nullptr;
        std::array<Rml::Element*, 4> level_labels{};
        std::array<Rml::Element*, 4> fills{};
        std::array<Rml::Element*, 4> rows{};
        Rml::Element* info_row = nullptr;
    };

    void LoadDocument();
    void WireHotbarSlots();
    void WireEventLogScroll();

    // Attaches mousedown/mousemove/mousescroll RmlEventListeners to hud.rml's
    // full-screen body, publishing WorldMouseDownMessage/WorldMouseMoveMessage/
    // WorldMouseScrollMessage for GameplayLayer to resolve against its own
    // Camera/Grid -- this layer holds no gameplay state of its own (see class
    // doc comment), so it never interprets these itself. RmlUi's own listener
    // dispatch fires independent of Application::Run's native/semantic event
    // pass, so no pointer-events CSS changes are needed (mirrors
    // Editor/Source/Layers/PieceEditorLayer.cpp's WireGridInteraction).
    void WireWorldMouseInteraction();

    void OnPlayerStatus(const PlayerStatusMessage& message);
    void OnHotbarState(const HotbarStateMessage& message);
    void OnLogEntry(const CombatLogEntryMessage& message);
    void OnStatusEffects(const StatusEffectsMessage& message);
    void OnPlayerDefeated(const PlayerDefeatedMessage& message);
    void OnGameRestarted(const GameRestartedMessage& message);
    void OnLootDrop(const LootDropMessage& message);
    void OnMissionCompleted(const MissionCompletedMessage& message);

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

    // GameplayLayer's response to UpdateStatPreview's request -- see
    // CharacterScreenStatPreviewMessage's own doc comment. Caches the deltas
    // and re-renders the stats panel; a stale response that no longer matches
    // the current hover/focus target is harmless (the next UpdateStatPreview
    // call re-requests and overwrites it), so this doesn't bother correlating
    // request/response by index.
    void OnStatPreview(const CharacterScreenStatPreviewMessage& message);

    // Same shape as OnCharacterScreenState/OnCharacterScreenClosed, for the
    // Techniques/Photon Arts screen -- two panels, no Stats-equivalent, no
    // context menu (a row's only action is "assign to hotbar", so Space goes
    // straight into the awaiting-slot sub-state via ActivateFocusedTechRow).
    void OnActionPaletteState(const ActionPaletteMessage& message);
    void OnActionPaletteClosed(const ActionPaletteClosedMessage& message);

    // Single-panel variant of OnCharacterScreenState/OnActionPaletteState
    // for Mission Select -- a flat row list (no panel split), a locked row
    // renders dimmed and can't be focused/selected. Space on a focused
    // unlocked row publishes MissionSelectedMessage directly (no context
    // menu, no awaiting-hotbar-slot sub-state).
    void OnMissionSelectState(const MissionSelectMessage& message);
    void OnMissionSelectClosed(const MissionSelectClosedMessage& message);
    int MissionSelectRowCount() const;
    void MoveMissionSelectRowFocus(int direction);
    void ActivateFocusedMissionSelectRow();
    void RenderMissionSelectFocusHighlight();

    // Same two-panel, no-context-menu shape as OnActionPaletteState, for
    // the Shop screen (Stock / Your Items). Space on a focused Stock row
    // publishes ShopBuyRequestedMessage; on a focused Sellable row,
    // ShopSellRequestedMessage.
    void OnShopScreenState(const ShopMessage& message);
    void OnShopScreenClosed(const ShopClosedMessage& message);
    int ShopScreenRowCount(ShopScreenPanel panel) const;
    void MoveShopPanelFocus(int direction);
    void MoveShopRowFocus(int direction);
    void ActivateFocusedShopRow();
    void RenderShopFocusHighlights();
    void RenderShopRowFocus(const char* container_id, const char* row_class, ShopScreenPanel panel);

    // Same shape again, for the Storage screen (Inventory / Storage). Space
    // on a focused Inventory row publishes StorageItemActivatedMessage; on a
    // focused Storage row, StorageWithdrawRequestedMessage.
    void OnStorageScreenState(const StorageMessage& message);
    void OnStorageScreenClosed(const StorageClosedMessage& message);
    int StorageScreenRowCount(StorageScreenPanel panel) const;
    void MoveStoragePanelFocus(int direction);
    void MoveStorageRowFocus(int direction);
    void ActivateFocusedStorageRow();
    void RenderStorageFocusHighlights();
    void RenderStorageRowFocus(const char* container_id, const char* row_class, StorageScreenPanel panel);

    // Shows/hides the hub interaction hint ("Press SPACE to ...") keyed off
    // the current InteractionType, or hides it entirely on nullopt.
    void OnHubInteractionPrompt(const HubInteractionPromptMessage& message);

    // Dungeon-scene sibling of OnHubInteractionPrompt -- shows/hides the same
    // hint element keyed off TeleporterDestination instead (mutually
    // exclusive scenes, so the two never fight over it).
    void OnTeleporterPrompt(const TeleporterPromptMessage& message);

    // Rebuilds #floating-text-layer every call (published every frame by
    // GameplayLayer) -- one positioned, non-interactive anchor div per active
    // FloatingTextSystem instance (see .floating-text-anchor in hud.rcss),
    // wrapping the actual text span; left/top/color set inline since they're
    // per-instance, not shared CSS.
    void OnFloatingTextState(const FloatingTextStateMessage& message);

    // Shows/hides #target-panel and, while shown, sets the target's name/
    // race text and HP bar fill -- same PercentWidth/EscapeRml idioms as
    // OnPlayerStatus's own bar update.
    void OnTargetState(const TargetStateMessage& message);

    // Shows/hides #tile-tooltip near the last known mouse position (cached
    // from the mousemove listener that also publishes WorldMouseMoveMessage,
    // see WireWorldMouseInteraction), labelled from message.label.
    void OnWorldTileHover(const WorldTileHoverMessage& message);

    void AppendLogLine(const std::string& text);

    // Recomputes each visible .log-line's opacity from its position in the
    // #event-log scroll viewport (newest visible = 1.0, oldest visible =
    // 0.25, interpolated between). Run once layout has caught up with the
    // latest SetInnerRML (see m_log_scroll_pending) and again on every
    // manual scroll of the log, via m_log_scroll_listener.
    void UpdateLogLineOpacities();

    // Character-screen navigation/context-menu helpers -- see OnEvent's doc
    // comment. Split out of OnEvent itself so both the keyboard path and the
    // RmlClickListener callbacks on rows/menu options can share them.
    // Renders #character-screen-stats from m_character_screen_cache->stats,
    // annotated with m_stat_preview's deltas (if active) -- "ATP: 45 -> 50" in
    // the increase/decrease color, plain "ATP: 45" otherwise. Reads both
    // members directly (no parameters) since every caller already has both in
    // sync -- see UpdateStatPreview's doc comment for who calls this and when.
    void RenderStatsPanel();
    int CharacterScreenRowCount(CharacterScreenPanel panel) const;
    void MovePanelFocus(int direction);
    void MoveRowFocus(int direction);
    void ActivateFocusedRow();
    void OpenContextMenu(CharacterScreenPanel panel, int index);
    void CloseContextMenu();
    void MoveMenuHighlight(int direction);
    void ChooseHighlightedMenuOption();
    void JumpToMatchingInventoryItem(EquipmentSlot slot);

    // "Feed" sub-state, entered from ChooseHighlightedMenuOption's Feed case
    // instead of publishing immediately (same "close the menu, enter a
    // distinguishable modal sub-state" shape as BeginAwaitingHotbarSlot, but
    // unlike it, normal Inventory row navigation stays live -- see OnEvent's
    // m_awaiting_mag_food_selection branch). Jumps focus to the first
    // is_mag_food Inventory row, if any. ActivateFocusedRow, while this is
    // set, publishes MagFeedRequestedMessage for an is_mag_food row instead
    // of opening that row's own context menu.
    void BeginAwaitingMagFood();
    void CancelAwaitingMagFood();

    // Builds/refreshes #character-screen-mag-panel from
    // m_character_screen_cache->mag. Builds the panel's row markup once
    // (cached into m_mag_panel_elements) and thereafter only updates text/
    // width on those same elements -- rebuilding via SetInnerRML every call,
    // the old approach, destroyed and recreated the .mag-stat-bar-fill
    // elements each time, so there was never a previous frame's width for an
    // RCSS transition to animate from. Retargets (doesn't reset)
    // m_mag_stat_animations from the cache's actual level/progress; the
    // fill-over-time animation itself is advanced in UpdateMagPanelAnimations,
    // called every frame from OnUpdate so it keeps progressing regardless of
    // focus/hover churn. Visibility is delegated to RefreshMagPanelVisibility.
    void RenderMagPanel();

    // Pushes the four MagStatAnimation entries' current displayed_subunits/
    // displayed_level/level_up_flash into m_mag_panel_elements (title text,
    // per-stat level text + wrapped-progress fill width, the "mag-level-up"
    // flash class, IQ/Sync text). Called after RenderMagPanel retargets and
    // again every tick from UpdateMagPanelAnimations -- a no-op if the panel
    // markup hasn't been built yet.
    void ApplyMagPanelDisplay();

    // Advances each of the four MagStatAnimation entries toward its
    // target_subunits at a fixed rate (see kMagBarFillUnitsPerSecond in the
    // .cpp), wrapping displayed_level up by one and arming level_up_flash
    // whenever displayed_subunits crosses a progress_to_next boundary --
    // this is what makes a multi-level feed visibly fill to 100%, flash, and
    // reset per level instead of jumping straight to the final bar position.
    // Ticks level_up_flash back down toward zero regardless. A no-op once
    // AnyMagStatAnimating() is false, so this costs nothing while idle.
    void UpdateMagPanelAnimations(float delta_time);

    // True while any of the four stats still has displayed_subunits short of
    // target_subunits, or a level_up_flash still fading -- keeps the mag
    // panel visible (see RefreshMagPanelVisibility) for the whole animation
    // even after focus/hover has moved off the Mag slot (e.g. once
    // ActivateFocusedRow's Feed flow cancels awaiting-mag-food and moves
    // focus back to a plain Inventory row).
    bool AnyMagStatAnimating() const;

    // Shows/hides #character-screen-mag-panel: visible while the Equipment
    // panel's hover/focus target is the Mag slot, OR the "select food to
    // feed" sub-state is active (m_awaiting_mag_food_selection), OR
    // AnyMagStatAnimating() -- so the panel stays up for the whole feed
    // animation instead of disappearing the instant focus leaves the Mag row
    // (which BeginAwaitingMagFood always does, to jump focus into Inventory).
    void RefreshMagPanelVisibility();

    // Renders #character-screen-item-detail from whichever Inventory/
    // Equipment row is currently hovered (taking priority) or keyboard-
    // focused -- same target-resolution shape as UpdateStatPreview, except
    // not restricted to equippable rows (a consumable/mod should still show
    // its detail). Hidden (via display:none) when nothing resolves. Reads
    // straight from m_character_screen_cache (every field the panel needs --
    // name, stars, description, equip slot, stats, species bonuses, a
    // weapon's range/targeting/grind/status-chance/granted Photon Arts, an
    // armor's mod slot count -- is already resolved there), so unlike
    // UpdateStatPreview this never needs a round-trip message.
    void RenderItemDetailPanel();

    // Stat-change hover preview: recomputes which Inventory or Equipment row
    // (if any) is the current "preview target" -- an active mouse hover in
    // either panel takes priority over the keyboard-focused row, which is
    // used when nothing is hovered. An Inventory target previews the delta
    // from equipping that row (ComputeEquipStatDelta, via
    // InventoryItemHoverChangedMessage); an Equipment target previews the
    // delta from removing whatever occupies that slot
    // (ComputeUnequipStatDelta, via EquipmentSlotHoverChangedMessage). Only
    // publishes when the resolved target actually changed since the last
    // call (GameplayLayer's OnStatPreview response updates m_stat_preview and
    // re-renders). Called from every place a hover or keyboard focus can
    // change: the hover listeners below, and
    // RenderFocusHighlights/OnCharacterScreenState/OnCharacterScreenClosed.
    void UpdateStatPreview();

    // Techniques-screen navigation helpers -- same split-out-of-OnEvent
    // reasoning as the Character-screen helpers above, just without a
    // context-menu layer (a row has exactly one action).
    int ActionPaletteRowCount(ActionPalettePanel panel) const;
    void MoveTechPanelFocus(int direction);
    void MoveTechRowFocus(int direction);
    void ActivateFocusedTechRow();
    void RenderTechniquesFocusHighlights();
    void RenderTechRowFocus(const char* container_id, const char* row_class, ActionPalettePanel panel);

    // "Assign to Hotbar" sub-state: menu closes, hint text changes, and
    // OnEvent waits for a 0-9 keypress (or Escape to cancel) instead of the
    // usual numpad navigation -- see OnEvent's doc comment. Shared between
    // the Character screen's inventory-item flow and the Techniques screen's
    // ability flow (see HotbarAssignSource).
    void BeginAwaitingHotbarSlot(int inventory_index);
    void BeginAwaitingAbilityHotbarSlot(HotbarSlotType type, std::uint32_t id);
    void CancelAwaitingHotbarSlot();
    void SetCharacterScreenHint(const char* text, bool awaiting);
    void SetActionPaletteHint(const char* text, bool awaiting);
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

    // One RmlHoverListener per .inventory-row and .equip-row, rebuilt
    // alongside m_character_screen_listeners -- separate container/class
    // since a row needs both a click listener (context menu) and a hover
    // listener (stat preview) simultaneously.
    std::vector<std::unique_ptr<RmlHoverListener>> m_character_screen_hover_listeners;

    // Set by the hover listeners above; nullopt when the mouse isn't over any
    // inventory/equipment row, respectively. See UpdateStatPreview's doc
    // comment.
    std::optional<int> m_hovered_inventory_index;
    std::optional<int> m_hovered_equipment_index;

    // The target last sent via InventoryItemHoverChangedMessage/
    // EquipmentSlotHoverChangedMessage (or nullopt for "no preview"), so
    // UpdateStatPreview only re-publishes when the target actually changes.
    std::optional<int> m_requested_preview_inventory_index;
    std::optional<EquipmentSlot> m_requested_preview_equipment_slot;

    // Set by BeginAwaitingMagFood, cleared by CancelAwaitingMagFood (also
    // called, harmlessly, from every screen open/close path -- same
    // "unconditional guarded reset" idiom as CancelAwaitingHotbarSlot).
    bool m_awaiting_mag_food_selection = false;

    // POW/DEF/DEX/MIND fill-over-time animation state (see
    // UpdateMagPanelAnimations) and the cached mag-panel child elements they
    // drive (see RenderMagPanel). m_mag_panel_elements is nullopt until the
    // panel's row markup has been built at least once, and is reset whenever
    // the mag panel goes back to having nothing to show (mag unequipped) or
    // the Character screen closes, so the next equip/open rebuilds fresh and
    // snaps to the new mag's actual values instead of animating from a stale
    // baseline.
    std::array<MagStatAnimation, 4> m_mag_stat_animations;
    std::optional<MagPanelElements> m_mag_panel_elements;

    // Latest CharacterScreenStatPreviewMessage; nullopt (rendered as no
    // preview) until the first response arrives after a preview target is
    // requested. Cleared in OnCharacterScreenClosed.
    std::optional<CharacterScreenStatPreviewMessage> m_stat_preview;

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

    // Set by OnCharacterScreenState instead of calling OpenContextMenu
    // directly when a feed-to-exhaustion refresh needs to land back on the
    // mag's own context menu -- the equipment rows were just rebuilt via
    // SetInnerRML in that same call, so RenderContextMenu's anchor->
    // GetAbsoluteOffset() would still read pre-layout geometry. Consumed at
    // the top of the next OnUpdate, same pattern as m_log_scroll_pending.
    bool m_reopen_mag_context_menu_pending = false;

    // Rebuilt on every OnActionPaletteState call -- same reasoning as
    // m_character_screen_listeners (row count isn't fixed).
    std::vector<std::unique_ptr<RmlClickListener>> m_action_palette_listeners;

    // Same "cache drives OnEvent interception, empty means closed" contract
    // as m_character_screen_cache. Set in OnActionPaletteState, cleared in
    // OnActionPaletteClosed.
    std::optional<ActionPaletteMessage> m_action_palette_cache;

    ActionPalettePanel m_tech_focused_panel = ActionPalettePanel::Techniques;
    int m_tech_focused_row = 0;

    // Rebuilt on every OnMissionSelectState call -- same reasoning as
    // m_character_screen_listeners.
    std::vector<std::unique_ptr<RmlClickListener>> m_mission_select_listeners;

    // Same "cache drives OnEvent interception, empty means closed" contract
    // as m_character_screen_cache.
    std::optional<MissionSelectMessage> m_mission_select_cache;
    int m_mission_select_focused_row = 0;

    // Same three members, for the Shop screen.
    std::vector<std::unique_ptr<RmlClickListener>> m_shop_listeners;
    std::optional<ShopMessage> m_shop_cache;
    ShopScreenPanel m_shop_focused_panel = ShopScreenPanel::Stock;
    int m_shop_focused_row = 0;

    // Same again, for the Storage screen.
    std::vector<std::unique_ptr<RmlClickListener>> m_storage_listeners;
    std::optional<StorageMessage> m_storage_cache;
    StorageScreenPanel m_storage_focused_panel = StorageScreenPanel::Inventory;
    int m_storage_focused_row = 0;

    // Set by BeginAwaitingHotbarSlot/BeginAwaitingAbilityHotbarSlot ("Assign
    // to Hotbar" chosen, waiting on a 0-9 keypress); mutually exclusive with
    // m_menu_open. Which of the two payload fields below is meaningful
    // depends on the source.
    HotbarAssignSource m_awaiting_hotbar_assign_source = HotbarAssignSource::None;
    int m_awaiting_hotbar_inventory_index = -1;                            // CharacterScreenItem
    HotbarSlotType m_awaiting_hotbar_ability_type = HotbarSlotType::Empty; // ActionPaletteAbility
    std::uint32_t m_awaiting_hotbar_ability_id = 0;                        // ActionPaletteAbility

    static constexpr std::size_t kMaxLogLines = 50;
    std::deque<std::string> m_log_lines; // markup text (see LogMarkup.h) from CombatLogEntryMessage/LootDropMessage

    // Set by AppendLogLine after rebuilding #event-log's markup; consumed at
    // the top of the next OnUpdate, once Application's frame loop has run
    // Context::Update() and RmlUi's layout has actually caught up with that
    // markup (GetScrollHeight() read synchronously in AppendLogLine would
    // still reflect the previous frame's layout).
    bool m_log_scroll_pending = false;
    std::unique_ptr<RmlScrollListener> m_log_scroll_listener;

    std::vector<std::unique_ptr<RmlEventListener>> m_world_mouse_listeners;

    // Cached by the mousemove listener, read by OnWorldTileHover to position
    // #tile-tooltip -- the listener itself only has the Rml::Event, not
    // anywhere else to stash the position for a later, separate message.
    float m_last_mouse_screen_x = 0.0f;
    float m_last_mouse_screen_y = 0.0f;
};

} // namespace psr
