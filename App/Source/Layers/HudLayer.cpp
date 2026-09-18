#include "Layers/HudLayer.h"

#include "Messages/CharacterScreenClosedMessage.h"
#include "Messages/CharacterScreenMessage.h"
#include "Messages/CharacterScreenStatPreviewMessage.h"
#include "Messages/CombatLogEntryMessage.h"
#include "Messages/ConfirmChoiceMessage.h"
#include "Messages/ConfirmClosedMessage.h"
#include "Messages/EquipmentSlotActivatedMessage.h"
#include "Messages/EquipmentSlotHoverChangedMessage.h"
#include "Messages/FloatingTextStateMessage.h"
#include "Messages/GameRestartedMessage.h"
#include "Messages/HotbarSlotActivatedMessage.h"
#include "Messages/HotbarSlotAssignedMessage.h"
#include "Messages/HotbarStateMessage.h"
#include "Messages/HubInteractionPromptMessage.h"
#include "Messages/HudReadyMessage.h"
#include "Messages/InventoryItemActivatedMessage.h"
#include "Messages/InventoryItemHoverChangedMessage.h"
#include "Messages/LootDropMessage.h"
#include "Messages/MagFeedRequestedMessage.h"
#include "Messages/MissionCompletedMessage.h"
#include "Messages/MissionSelectClosedMessage.h"
#include "Messages/MissionSelectedMessage.h"
#include "Messages/PauseMenuActionMessage.h"
#include "Messages/PauseMenuClosedMessage.h"
#include "Messages/PlayerDefeatedMessage.h"
#include "Messages/PlayerStatusMessage.h"
#include "Messages/ReturnedToTitleMessage.h"
#include "Messages/ShopBuyRequestedMessage.h"
#include "Messages/ShopClosedMessage.h"
#include "Messages/ShopSellRequestedMessage.h"
#include "Messages/StatusEffectsMessage.h"
#include "Messages/StorageClosedMessage.h"
#include "Messages/StorageItemActivatedMessage.h"
#include "Messages/StorageWithdrawRequestedMessage.h"
#include "Messages/TargetStateMessage.h"
#include "Messages/ActionPaletteClosedMessage.h"
#include "Messages/ActionPaletteSlotAssignedMessage.h"
#include "Messages/TeleporterPromptMessage.h"
#include "Messages/WorldMouseDownMessage.h"
#include "Messages/WorldMouseMoveMessage.h"
#include "Messages/WorldMouseScrollMessage.h"
#include "Messages/WorldTileHoverMessage.h"

#include "ApplicationFilepaths.h"
#include "Components/WeaponComponent.h" // WeaponRangeShape
#include "Engine/Combat/TargetingMode.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Math/Color.h"
#include "Items/Equip.h"
#include "UI/LogMarkup.h"
#include "UI/RmlClickListener.h"
#include "UI/RmlEventListener.h"
#include "UI/RmlHoverListener.h"
#include "UI/RmlScrollListener.h"
#include "UI/RmlText.h"

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace psr {

namespace {

    // Must match #character-screen-context-menu's width/max-height in
    // hud.rcss -- RenderContextMenu positions using these fixed constants
    // rather than the element's own live box, to avoid a same-call
    // pre-layout read (same reasoning as PieceEditorLayer's painter dropdown).
    constexpr float kContextMenuWidth = 180.0f;
    constexpr float kContextMenuMaxHeight = 200.0f;

    // Baseline size floating text renders at when Camera::GetZoom() == 1 --
    // multiplied by each entry's scale so text grows with zoom instead of
    // staying a fixed screen size while the world underneath it magnifies.
    constexpr float kFloatingTextBaseFontSizeEm = 0.9f;

    // Baseline anchor box size (Camera::GetZoom() == 1), scaled by each
    // entry's scale alongside the font size -- generously larger than any
    // damage/heal number the font size ever produces, so .floating-text-anchor
    // (see hud.rcss) always centers its child within real, positive space
    // instead of relying on a flex container overflowing negative free space
    // (RmlUi's flex layout doesn't center overflow past a zero-size
    // container -- it left/top-aligns instead).
    constexpr float kFloatingTextAnchorBaseWidth = 160.0f;
    constexpr float kFloatingTextAnchorBaseHeight = 32.0f;

    // Must match #character-screen-hint's initial text in hud.rml -- swapped
    // back in by CancelAwaitingHotbarSlot, same "must match markup"
    // reasoning as kContextMenuWidth/kContextMenuMaxHeight above.
    constexpr const char* kDefaultCharacterScreenHint = "Numpad to navigate, Space to select, C / Esc to close";
    constexpr const char* kAwaitingHotbarSlotHint = "Press 0-9 to assign to a hotbar slot (Esc to cancel)";
    constexpr const char* kAwaitingMagFoodHint = "Select a food item to feed the mag (Esc to cancel)";

    // Must match #action-palette-hint's initial text in hud.rml -- same
    // "must match markup" reasoning as kDefaultCharacterScreenHint.
    constexpr const char* kDefaultActionPaletteHint =
        "Numpad to navigate, Space to assign to hotbar, P / Esc to close";

    // Number-row key to hotbar slot index: 1-9 -> 0-8, 0 -> 9. Mirrors
    // GameplayLayer.cpp's own KeyCodeToHotbarSlot -- duplicated rather than
    // shared since it's a single 6-line pure function used from two
    // otherwise-unrelated translation units, per CLAUDE.md's "three similar
    // lines is better than a premature abstraction."
    std::optional<int> KeyCodeToHotbarSlot(int key_code)
    {
        if (key_code >= SDLK_1 && key_code <= SDLK_9)
            return key_code - SDLK_1;
        if (key_code == SDLK_0)
            return 9;
        return std::nullopt;
    }

    // display_name, plus " xN" when the item is a stack of more than one
    // (see CharacterScreenMessage::ItemEntry::quantity).
    std::string ItemRowLabel(const CharacterScreenMessage::ItemEntry& entry)
    {
        return entry.quantity > 1 ? entry.display_name + " x" + std::to_string(entry.quantity) : entry.display_name;
    }

    std::string PercentWidth(int current, int max)
    {
        const int clamped_max = std::max(max, 1);
        const int percent = std::clamp(current * 100 / clamped_max, 0, 100);
        return std::to_string(percent) + "%";
    }

    // A short display label + an rcss class per StatusEffectType -- rcss
    // (hud.rcss's .status-chip.status-<class> rules) owns the actual color,
    // this only decides which rule applies. Purely presentational, kept
    // local to HudLayer rather than on StatusEffectType itself (an engine
    // enum has no business knowing about UI class names).
    std::pair<const char*, const char*> StatusEffectDisplay(StatusEffectType type)
    {
        switch (type)
        {
        case StatusEffectType::Poison:
            return {"Poison", "poison"};
        case StatusEffectType::Burn:
            return {"Burn", "burn"};
        case StatusEffectType::Freeze:
            return {"Freeze", "freeze"};
        case StatusEffectType::Shock:
            return {"Shock", "shock"};
        case StatusEffectType::Confuse:
            return {"Confuse", "confuse"};
        }
        return {"?", "poison"}; // unreachable for a valid enum value
    }

    // No Color -> CSS-string conversion exists anywhere yet -- every other
    // HUD color is a fixed hud.rcss rule, never a per-instance runtime value.
    // RmlUi's rgba() takes all four channels as 0-255 integers (unlike CSS3's
    // fractional alpha) -- matches every existing rgba(...) in hud.rcss.
    std::string ColorToRgbaCss(Color color)
    {
        return "rgba(" + std::to_string(color.r) + "," + std::to_string(color.g) + "," + std::to_string(color.b) + "," +
               std::to_string(color.a) + ")";
    }

    // "LABEL: 45" normally, or "LABEL: 45 <span class=...>-> 50</span>" while
    // a stat preview is active and this stat actually moves -- delta == 0
    // (preview active but this particular stat unaffected, e.g. a weapon's
    // ATP-only bonus leaving DFP alone) renders the same as no preview.
    std::string StatRow(const char* label, int value, int delta)
    {
        std::string row = std::string("<div class=\"stat-row\">") + label + ": " + std::to_string(value);
        if (delta != 0)
        {
            const char* color_class = delta > 0 ? "stat-increase" : "stat-decrease";
            row += std::string(" <span class=\"") + color_class + "\">-> " + std::to_string(value + delta) + "</span>";
        }
        return row + "</div>";
    }

    // Index-aligned with EquipmentSlot (Weapon, Head, Torso, Hands, Legs, Mag)
    // -- shared by OnCharacterScreenState's equip-row markup and
    // RenderItemDetailPanel's "Equip Slot: ..." line, so both name a slot the
    // same way.
    constexpr std::array<const char*, 6> kEquipSlotLabels = {"Weapon", "Head", "Torso", "Hands", "Legs", "Mag"};

    // "N filled star" glyphs for RarityComponent::stars -- no fixed max scale
    // (RarityComponent itself enforces no upper bound), so this renders
    // exactly `stars` glyphs rather than filling a fixed-size 5-star bar.
    // U+2726 (not U+2605 BLACK STAR) -- PixelCode-Regular.ttf's cmap has no
    // glyph for U+2605 (renders as a tofu box), but does cover U+2726.
    std::string StarsMarkup(int stars)
    {
        std::string markup;
        for (int i = 0; i < stars; ++i)
            markup += "\xE2\x9C\xA6"; // U+2726 BLACK FOUR POINTED STAR
        return markup;
    }

    // A short, human-readable label for a weapon's WeaponRangeShape --
    // includes the tile range/hit count only for the shapes where those
    // fields are actually meaningful (see WeaponRangeShape's own doc comment
    // on WeaponComponent.h).
    std::string RangeShapeLabel(WeaponRangeShape shape, int range, int hits_per_turn)
    {
        switch (shape)
        {
        case WeaponRangeShape::SingleTarget:
            return "Adjacent tile";
        case WeaponRangeShape::Cone3:
            return "3-tile cone";
        case WeaponRangeShape::Surrounding:
            return "All adjacent tiles";
        case WeaponRangeShape::Line:
        {
            std::string label = "Line, " + std::to_string(range) + " tile" + (range == 1 ? "" : "s");
            if (hits_per_turn > 1)
                label += ", " + std::to_string(hits_per_turn) + " hits/turn";
            return label;
        }
        }
        return "?"; // unreachable for a valid enum value
    }

    const char* TargetingModeLabel(TargetingMode mode)
    {
        switch (mode)
        {
        case TargetingMode::Directional:
            return "Directional (swing toward facing)";
        case TargetingMode::TargetSquare:
            return "Tile select";
        case TargetingMode::SelfTarget:
            return "Self";
        }
        return "?"; // unreachable for a valid enum value
    }
} // namespace

HudLayer::HudLayer() : Layer("HudLayer") {}
HudLayer::~HudLayer() = default;

void HudLayer::OnAttach()
{
    const std::filesystem::path font_path = ApplicationFilepaths::FontsPath / "PixelCode-Regular.ttf";
    if (!Rml::LoadFontFace(font_path.string().c_str()))
        SDL_Log("Warning: HudLayer failed to load font '%s'", font_path.string().c_str());

    LoadDocument();
    if (!m_document)
        return;

    WireHotbarSlots();
    WireEventLogScroll();
    WireWorldMouseInteraction();
    WirePauseMenu();
    WireConfirmDialog();

    Subscribe<PlayerStatusMessage>(&HudLayer::OnPlayerStatus, this);
    Subscribe<HotbarStateMessage>(&HudLayer::OnHotbarState, this);
    Subscribe<CombatLogEntryMessage>(&HudLayer::OnLogEntry, this);
    Subscribe<StatusEffectsMessage>(&HudLayer::OnStatusEffects, this);
    Subscribe<PlayerDefeatedMessage>(&HudLayer::OnPlayerDefeated, this);
    Subscribe<GameRestartedMessage>(&HudLayer::OnGameRestarted, this);
    Subscribe<LootDropMessage>(&HudLayer::OnLootDrop, this);
    Subscribe<CharacterScreenMessage>(&HudLayer::OnCharacterScreenState, this);
    Subscribe<CharacterScreenClosedMessage>(&HudLayer::OnCharacterScreenClosed, this);
    Subscribe<CharacterScreenStatPreviewMessage>(&HudLayer::OnStatPreview, this);
    Subscribe<ActionPaletteMessage>(&HudLayer::OnActionPaletteState, this);
    Subscribe<ActionPaletteClosedMessage>(&HudLayer::OnActionPaletteClosed, this);
    Subscribe<FloatingTextStateMessage>(&HudLayer::OnFloatingTextState, this);
    Subscribe<TargetStateMessage>(&HudLayer::OnTargetState, this);
    Subscribe<HubInteractionPromptMessage>(&HudLayer::OnHubInteractionPrompt, this);
    Subscribe<TeleporterPromptMessage>(&HudLayer::OnTeleporterPrompt, this);
    Subscribe<MissionCompletedMessage>(&HudLayer::OnMissionCompleted, this);
    Subscribe<MissionSelectMessage>(&HudLayer::OnMissionSelectState, this);
    Subscribe<MissionSelectClosedMessage>(&HudLayer::OnMissionSelectClosed, this);
    Subscribe<ShopMessage>(&HudLayer::OnShopScreenState, this);
    Subscribe<ShopClosedMessage>(&HudLayer::OnShopScreenClosed, this);
    Subscribe<StorageMessage>(&HudLayer::OnStorageScreenState, this);
    Subscribe<StorageClosedMessage>(&HudLayer::OnStorageScreenClosed, this);
    Subscribe<WorldTileHoverMessage>(&HudLayer::OnWorldTileHover, this);
    Subscribe<PauseMenuMessage>(&HudLayer::OnPauseMenuState, this);
    Subscribe<PauseMenuClosedMessage>(&HudLayer::OnPauseMenuClosed, this);
    Subscribe<ConfirmMessage>(&HudLayer::OnConfirmState, this);
    Subscribe<ConfirmClosedMessage>(&HudLayer::OnConfirmClosed, this);
    Subscribe<ReturnedToTitleMessage>(&HudLayer::OnReturnedToTitle, this);

    // Tells GameplayLayer to re-publish current state now that this layer is
    // actually subscribed -- see HudReadyMessage.h for why a one-time publish
    // from GameplayLayer::OnAttach can't reach this layer directly.
    Publish(HudReadyMessage{});
}

void HudLayer::OnDetach()
{
    m_hotbar_listeners.clear();
    m_character_screen_listeners.clear();
    m_character_screen_hover_listeners.clear();
    m_action_palette_listeners.clear();
    m_mission_select_listeners.clear();
    m_shop_listeners.clear();
    m_storage_listeners.clear();
    m_pause_listeners.clear();
    m_confirm_listeners.clear();
    m_context_menu_listeners.clear();
    m_log_scroll_listener.reset();
    m_world_mouse_listeners.clear();
    if (m_document)
    {
        m_document->Close();
        m_document = nullptr;
    }
}

void HudLayer::OnUpdate(float delta_time)
{
    if (m_log_scroll_pending && m_document)
    {
        if (Rml::Element* log = m_document->GetElementById("event-log"))
        {
            log->SetScrollTop(log->GetScrollHeight());
            UpdateLogLineOpacities();
        }
        m_log_scroll_pending = false;
    }

    if (m_reopen_mag_context_menu_pending)
    {
        m_reopen_mag_context_menu_pending = false;
        OpenContextMenu(CharacterScreenPanel::Equipment, static_cast<int>(EquipmentSlot::Mag));
    }

    UpdateMagPanelAnimations(delta_time);

    HandleQueuedMessages();
}

void HudLayer::LoadDocument()
{
    const std::filesystem::path document_path = ApplicationFilepaths::RmlDocumentsPath / "hud.rml";
    GuiContext::LockedAccess gui_context = GetLockedGuiContext();
    m_document = gui_context->LoadDocument(document_path.string().c_str());
    if (!m_document)
    {
        SDL_Log("Warning: HudLayer has no document to show");
        return;
    }
    m_document->Show();
}

void HudLayer::WireHotbarSlots()
{
    constexpr int kHotbarSlotCount = 10;
    for (int slot = 0; slot < kHotbarSlotCount; ++slot)
    {
        Rml::Element* element = m_document->GetElementById("hotbar-slot-" + std::to_string(slot));
        if (!element)
            continue;

        auto listener =
            std::make_unique<RmlClickListener>([this, slot]() { Publish(HotbarSlotActivatedMessage{slot}); });
        listener->Attach(*element);
        m_hotbar_listeners.push_back(std::move(listener));
    }
}

void HudLayer::WirePauseMenu()
{
    static constexpr std::array<std::pair<const char*, PauseMenuAction>, 3> kActionRows = {
        {{"pause-row-resume", PauseMenuAction::Resume},
         {"pause-row-quit-title", PauseMenuAction::QuitToTitle},
         {"pause-row-quit-desktop", PauseMenuAction::QuitToDesktop}}};
    for (const auto& [id, action] : kActionRows)
    {
        Rml::Element* element = m_document->GetElementById(id);
        if (!element)
            continue;
        auto listener = std::make_unique<RmlClickListener>(
            [this, action]()
            {
                if (!m_pause_cache || m_pause_placeholder_open || m_confirm_cache)
                    return;
                Publish(PauseMenuActionMessage{action});
            });
        listener->Attach(*element);
        m_pause_listeners.push_back(std::move(listener));
    }

    static constexpr std::array<std::pair<const char*, const char*>, 2> kPlaceholderRows = {
        {{"pause-row-options", "Options"}, {"pause-row-help", "Help"}}};
    for (const auto& [id, title] : kPlaceholderRows)
    {
        Rml::Element* element = m_document->GetElementById(id);
        if (!element)
            continue;
        auto listener = std::make_unique<RmlClickListener>(
            [this, title]()
            {
                if (!m_pause_cache || m_pause_placeholder_open || m_confirm_cache)
                    return;
                ShowPausePlaceholder(title);
            });
        listener->Attach(*element);
        m_pause_listeners.push_back(std::move(listener));
    }

    if (Rml::Element* back = m_document->GetElementById("pause-placeholder-hint"))
    {
        auto listener = std::make_unique<RmlClickListener>([this]() { HidePausePlaceholder(); });
        listener->Attach(*back);
        m_pause_listeners.push_back(std::move(listener));
    }
}

void HudLayer::WireConfirmDialog()
{
    static constexpr std::array<std::pair<const char*, bool>, 2> kRows = {
        {{"confirm-row-yes", true}, {"confirm-row-no", false}}};
    for (const auto& [id, confirmed] : kRows)
    {
        Rml::Element* element = m_document->GetElementById(id);
        if (!element)
            continue;
        auto listener = std::make_unique<RmlClickListener>(
            [this, confirmed]()
            {
                if (!m_confirm_cache)
                    return;
                Publish(ConfirmChoiceMessage{confirmed});
            });
        listener->Attach(*element);
        m_confirm_listeners.push_back(std::move(listener));
    }
}

void HudLayer::WireEventLogScroll()
{
    Rml::Element* log = m_document->GetElementById("event-log");
    if (!log)
        return;

    m_log_scroll_listener = std::make_unique<RmlScrollListener>([this]() { UpdateLogLineOpacities(); });
    m_log_scroll_listener->Attach(*log);
}

void HudLayer::WireWorldMouseInteraction()
{
    Rml::Element* body = m_document->GetElementById("hud-body");
    if (!body)
        return;

    auto down = std::make_unique<RmlEventListener>("mousedown",
                                                    [this](Rml::Event& event)
                                                    {
                                                        WorldMouseDownMessage message;
                                                        message.screen_x = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
                                                        message.screen_y = static_cast<float>(event.GetParameter<int>("mouse_y", 0));
                                                        message.button = event.GetParameter<int>("button", 0);
                                                        Publish(message);
                                                    });
    down->Attach(*body);
    m_world_mouse_listeners.push_back(std::move(down));

    auto move = std::make_unique<RmlEventListener>("mousemove",
                                                    [this](Rml::Event& event)
                                                    {
                                                        WorldMouseMoveMessage message;
                                                        message.screen_x = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
                                                        message.screen_y = static_cast<float>(event.GetParameter<int>("mouse_y", 0));
                                                        m_last_mouse_screen_x = message.screen_x;
                                                        m_last_mouse_screen_y = message.screen_y;
                                                        Publish(message);
                                                    });
    move->Attach(*body);
    m_world_mouse_listeners.push_back(std::move(move));

    auto scroll = std::make_unique<RmlEventListener>("mousescroll",
                                                      [this](Rml::Event& event)
                                                      {
                                                          WorldMouseScrollMessage message;
                                                          message.wheel_delta_y = event.GetParameter<float>("wheel_delta_y", 0.0f);
                                                          Publish(message);
                                                      });
    scroll->Attach(*body);
    m_world_mouse_listeners.push_back(std::move(scroll));
}

void HudLayer::OnPlayerStatus(const PlayerStatusMessage& message)
{
    if (!m_document)
        return;

    if (Rml::Element* level_text = m_document->GetElementById("level-text"))
        level_text->SetInnerRML(EscapeRml("Lv" + std::to_string(message.level)));

    if (Rml::Element* hp_fill = m_document->GetElementById("hp-fill"))
        hp_fill->SetProperty("width", PercentWidth(message.current_hp, message.max_hp));
    if (Rml::Element* hp_text = m_document->GetElementById("hp-text"))
        hp_text->SetInnerRML(EscapeRml(std::to_string(message.current_hp) + " / " + std::to_string(message.max_hp)));

    if (Rml::Element* secondary_row = m_document->GetElementById("secondary-row"))
        secondary_row->SetProperty("display", message.has_secondary ? "flex" : "none");

    if (!message.has_secondary)
        return;

    if (Rml::Element* fill = m_document->GetElementById("secondary-fill"))
        fill->SetProperty("width", PercentWidth(message.current_secondary, message.max_secondary));
    if (Rml::Element* text = m_document->GetElementById("secondary-text"))
        text->SetInnerRML(
            EscapeRml(std::to_string(message.current_secondary) + " / " + std::to_string(message.max_secondary)));
}

void HudLayer::OnTargetState(const TargetStateMessage& message)
{
    if (!m_document)
        return;

    if (Rml::Element* panel = m_document->GetElementById("target-panel"))
        panel->SetProperty("display", message.has_target ? "block" : "none");

    if (!message.has_target)
        return;

    if (Rml::Element* name = m_document->GetElementById("target-name"))
        name->SetInnerRML(EscapeRml(message.name));
    if (Rml::Element* race = m_document->GetElementById("target-race"))
        race->SetInnerRML(EscapeRml(message.race_label));
    if (Rml::Element* fill = m_document->GetElementById("target-hp-fill"))
        fill->SetProperty("width", PercentWidth(message.current_hp, message.max_hp));
}

void HudLayer::OnWorldTileHover(const WorldTileHoverMessage& message)
{
    if (!m_document)
        return;

    Rml::Element* tooltip = m_document->GetElementById("tile-tooltip");
    if (!tooltip)
        return;

    if (!message.has_content)
    {
        tooltip->SetProperty("display", "none");
        return;
    }

    tooltip->SetInnerRML(EscapeRml(message.label));
    tooltip->SetProperty("left", std::to_string(m_last_mouse_screen_x + 12.0f) + "px");
    tooltip->SetProperty("top", std::to_string(m_last_mouse_screen_y + 12.0f) + "px");
    tooltip->SetProperty("display", "block");
}

void HudLayer::OnHubInteractionPrompt(const HubInteractionPromptMessage& message)
{
    if (!m_document)
        return;

    Rml::Element* prompt = m_document->GetElementById("hub-interaction-prompt");
    if (!prompt)
        return;

    if (!message.interaction_type)
    {
        prompt->SetProperty("display", "none");
        return;
    }

    const char* text = "Press SPACE to interact";
    switch (*message.interaction_type)
    {
    case InteractionType::Shop:
        text = "Press SPACE to shop";
        break;
    case InteractionType::Storage:
        text = "Press SPACE to access storage";
        break;
    case InteractionType::MissionSelect:
        text = "Press SPACE to select a mission";
        break;
    }
    prompt->SetInnerRML(text);
    prompt->SetProperty("display", "block");
}

void HudLayer::OnTeleporterPrompt(const TeleporterPromptMessage& message)
{
    if (!m_document)
        return;

    Rml::Element* prompt = m_document->GetElementById("hub-interaction-prompt");
    if (!prompt)
        return;

    if (!message.destination)
    {
        prompt->SetProperty("display", "none");
        return;
    }

    const char* text = "Press SPACE to interact";
    switch (*message.destination)
    {
    case TeleporterDestination::ReturnToHub:
        text = "Press SPACE to return to the Hub";
        break;
    case TeleporterDestination::AdvanceLevel:
        text = "Press SPACE to proceed to the next level";
        break;
    }
    prompt->SetInnerRML(text);
    prompt->SetProperty("display", "block");
}

void HudLayer::OnHotbarState(const HotbarStateMessage& message)
{
    if (!m_document)
        return;

    for (std::size_t slot = 0; slot < message.slots.size(); ++slot)
    {
        Rml::Element* element = m_document->GetElementById("hotbar-slot-" + std::to_string(slot));
        if (!element)
            continue;

        const HotbarStateMessage::SlotView& view = message.slots[slot];
        element->SetClass("slot-technique", view.type == HotbarSlotType::Technique);
        element->SetClass("slot-photon-art", view.type == HotbarSlotType::PhotonArt);
        element->SetClass("slot-item", view.type == HotbarSlotType::Item);
        element->SetClass("slot-normal-attack", view.type == HotbarSlotType::NormalAttack);
        element->SetClass("slot-special-attack", view.type == HotbarSlotType::SpecialAttack);

        if (Rml::Element* name = element->QuerySelector(".slot-name"))
            name->SetInnerRML(EscapeRml(view.name));
    }
}

void HudLayer::OnLogEntry(const CombatLogEntryMessage& message) { AppendLogLine(message.text); }

void HudLayer::AppendLogLine(const std::string& text)
{
    if (!m_document)
        return;

    m_log_lines.push_back(text);
    while (m_log_lines.size() > kMaxLogLines)
        m_log_lines.pop_front();

    Rml::Element* log = m_document->GetElementById("event-log");
    if (!log)
        return;

    std::string markup;
    for (const std::string& line : m_log_lines)
        markup +=
            "<div class=\"log-line\"><span class=\"log-prefix\">&gt; </span>" + ConvertLogMarkupToRml(line) + "</div>";
    log->SetInnerRML(markup);

    // Scrolling to bottom and recomputing opacities both need this frame's
    // layout to have caught up with the SetInnerRML above -- deferred to the
    // top of the next OnUpdate, see m_log_scroll_pending's doc comment.
    m_log_scroll_pending = true;
}

void HudLayer::UpdateLogLineOpacities()
{
    if (!m_document)
        return;

    Rml::Element* log = m_document->GetElementById("event-log");
    if (!log)
        return;

    Rml::ElementList lines;
    log->QuerySelectorAll(lines, ".log-line");
    if (lines.empty())
        return;

    const float scroll_top = log->GetScrollTop();
    const float client_height = log->GetClientHeight();

    std::optional<std::size_t> first_visible;
    std::optional<std::size_t> last_visible;
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const float top = lines[i]->GetOffsetTop();
        const float bottom = top + lines[i]->GetOffsetHeight();
        if (bottom <= scroll_top || top >= scroll_top + client_height)
            continue;

        if (!first_visible)
            first_visible = i;
        last_visible = i;
    }

    if (!first_visible || !last_visible)
    {
        first_visible = 0;
        last_visible = lines.size() - 1;
    }

    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        float opacity;
        if (i < *first_visible)
            opacity = 0.25f;
        else if (i > *last_visible)
            opacity = 1.0f;
        else
        {
            const std::size_t visible_count = *last_visible - *first_visible + 1;
            opacity =
                visible_count > 1
                    ? 0.25f + 0.75f * (static_cast<float>(i - *first_visible) / static_cast<float>(visible_count - 1))
                    : 1.0f;
        }

        lines[i]->SetProperty("opacity", std::to_string(opacity));
    }
}

void HudLayer::OnStatusEffects(const StatusEffectsMessage& message)
{
    if (!m_document)
        return;

    Rml::Element* row = m_document->GetElementById("status-effects");
    if (!row)
        return;

    if (message.active.empty())
    {
        row->SetInnerRML("");
        return;
    }

    std::string markup;
    for (const StatusEffectsMessage::ActiveEntry& entry : message.active)
    {
        const auto [label, css_class] = StatusEffectDisplay(entry.type);
        markup += std::string("<span class=\"status-chip status-") + css_class + "\">" + label + " x" +
                  std::to_string(entry.stacks) + " (" + std::to_string(entry.remaining_duration) + ")</span>";
    }
    row->SetInnerRML(markup);
}

void HudLayer::OnPlayerDefeated(const PlayerDefeatedMessage& /*message*/)
{
    if (!m_document)
        return;

    if (Rml::Element* overlay = m_document->GetElementById("game-over"))
        overlay->SetProperty("display", "flex");
}

void HudLayer::OnGameRestarted(const GameRestartedMessage& /*message*/)
{
    if (!m_document)
        return;

    if (Rml::Element* overlay = m_document->GetElementById("game-over"))
        overlay->SetProperty("display", "none");

    m_log_lines.clear();
    m_log_scroll_pending = false;
    if (Rml::Element* log = m_document->GetElementById("event-log"))
        log->SetInnerRML("");
}

void HudLayer::OnLootDrop(const LootDropMessage& message)
{
    AppendLogLine("Found [c=#d4c93f]" + message.item_name + "[/c]");
}

void HudLayer::OnMissionCompleted(const MissionCompletedMessage& message)
{
    AppendLogLine("Mission complete: [c=#7ee787]" + EscapeRml(message.dungeon_id_string) + "[/c]");
}

void HudLayer::OnCharacterScreenState(const CharacterScreenMessage& message)
{
    if (!m_document)
        return;

    const bool fresh_open = !m_character_screen_cache.has_value();
    const bool fed_this_refresh = message.fed_mag;
    m_character_screen_cache = message;
    CloseContextMenu();
    CancelAwaitingHotbarSlot();
    CancelAwaitingMagFood();

    if (Rml::Element* overlay = m_document->GetElementById("character-screen"))
        overlay->SetProperty("display", "flex");

    m_character_screen_listeners.clear();
    m_character_screen_hover_listeners.clear();
    // The DOM elements a mouse might currently be over are about to be torn
    // down and rebuilt below -- RmlUi will refire "mouseover" on whatever new
    // element ends up under the cursor on its next update, so there's nothing
    // to preserve here. m_requested_preview_inventory_index/
    // m_requested_preview_equipment_slot/m_stat_preview are reset too so
    // RenderFocusHighlights' end-of-function UpdateStatPreview call always
    // re-requests fresh deltas rather than trusting a preview computed
    // against pre-refresh stats (e.g. an equip that changed base stats but
    // left the still-hovered/focused row's index unchanged).
    m_hovered_inventory_index.reset();
    m_hovered_equipment_index.reset();
    m_requested_preview_inventory_index.reset();
    m_requested_preview_equipment_slot.reset();
    m_stat_preview.reset();

    RenderStatsPanel();

    if (Rml::Element* equipment_list = m_document->GetElementById("character-screen-equipment"))
    {
        std::string markup;
        for (std::size_t i = 0; i < message.equipment.size(); ++i)
        {
            const std::string label =
                message.equipment[i] ? EscapeRml(message.equipment[i]->display_name) : std::string("(empty)");
            std::string stars_markup;
            std::string mod_slots_markup;
            bool unmet_requirement = false;
            if (message.equipment[i])
            {
                if (message.equipment[i]->rarity_stars > 0)
                    stars_markup =
                        " <span class=\"item-detail-stars\">" + StarsMarkup(message.equipment[i]->rarity_stars) + "</span>";
                unmet_requirement = !message.equipment[i]->requirement_met;
                for (const std::string& mod_slot_label : message.equipment[i]->mod_slot_labels)
                    mod_slots_markup +=
                        "<div class=\"mod-slot-row\">\xE2\x80\xA2 " + EscapeRml(mod_slot_label) + "</div>";
            }
            markup += std::string("<div class=\"equip-row") + (unmet_requirement ? " unmet-requirement" : "") +
                      "\">" + kEquipSlotLabels[i] + ": " + label + stars_markup + "</div>" + mod_slots_markup;
        }
        equipment_list->SetInnerRML(markup);

        Rml::ElementList rows;
        equipment_list->QuerySelectorAll(rows, ".equip-row");
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const int index = static_cast<int>(i);
            auto listener = std::make_unique<RmlClickListener>(
                [this, index]() { OpenContextMenu(CharacterScreenPanel::Equipment, index); });
            listener->Attach(*rows[i]);
            m_character_screen_listeners.push_back(std::move(listener));

            auto hover_listener = std::make_unique<RmlHoverListener>(
                [this, index]()
                {
                    m_hovered_equipment_index = index;
                    UpdateStatPreview();
                    RefreshMagPanelVisibility();
                },
                [this, index]()
                {
                    if (m_hovered_equipment_index == index)
                    {
                        m_hovered_equipment_index.reset();
                        UpdateStatPreview();
                        RefreshMagPanelVisibility();
                    }
                });
            hover_listener->Attach(*rows[i]);
            m_character_screen_hover_listeners.push_back(std::move(hover_listener));
        }
    }

    if (Rml::Element* inventory_list = m_document->GetElementById("character-screen-inventory"))
    {
        std::string markup;
        for (const CharacterScreenMessage::ItemEntry& entry : message.inventory)
        {
            std::string stars_markup;
            if (entry.rarity_stars > 0)
                stars_markup = " <span class=\"item-detail-stars\">" + StarsMarkup(entry.rarity_stars) + "</span>";
            markup += std::string("<div class=\"inventory-row") + (entry.requirement_met ? "" : " unmet-requirement") +
                      "\">" + EscapeRml(ItemRowLabel(entry)) + stars_markup + "</div>";
        }
        // Pinned to the bottom, outside the .inventory-row rows above -- see
        // CharacterScreenMessage::meseta's doc comment for why this doesn't
        // consume a slot index or participate in row focus/selection.
        markup += "<div class=\"meseta-row\">Meseta: " + std::to_string(message.meseta) + "</div>";
        inventory_list->SetInnerRML(markup);

        Rml::ElementList rows;
        inventory_list->QuerySelectorAll(rows, ".inventory-row");
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const int index = static_cast<int>(i);
            auto listener = std::make_unique<RmlClickListener>(
                [this, index]()
                {
                    // While picking a food to feed the mag, a click behaves
                    // like Space on a keyboard-focused row (see OnEvent's
                    // m_awaiting_mag_food_selection branch) instead of
                    // opening that row's own context menu.
                    if (m_awaiting_mag_food_selection)
                    {
                        m_focused_panel = CharacterScreenPanel::Inventory;
                        m_focused_row = index;
                        ActivateFocusedRow();
                        return;
                    }
                    OpenContextMenu(CharacterScreenPanel::Inventory, index);
                });
            listener->Attach(*rows[i]);
            m_character_screen_listeners.push_back(std::move(listener));

            auto hover_listener = std::make_unique<RmlHoverListener>(
                [this, index]()
                {
                    m_hovered_inventory_index = index;
                    UpdateStatPreview();
                },
                [this, index]()
                {
                    if (m_hovered_inventory_index == index)
                    {
                        m_hovered_inventory_index.reset();
                        UpdateStatPreview();
                    }
                });
            hover_listener->Attach(*rows[i]);
            m_character_screen_hover_listeners.push_back(std::move(hover_listener));
        }
    }

    if (fresh_open)
    {
        m_focused_panel = CharacterScreenPanel::Stats;
        m_focused_row = 0;
    }
    else
    {
        m_focused_row = std::clamp(m_focused_row, 0, std::max(0, CharacterScreenRowCount(m_focused_panel) - 1));
    }
    RenderFocusHighlights();

    // Chain straight back into food selection after a successful feed
    // (message.fed_mag -- see its doc comment for why this reads off the
    // message rather than a flag set around the Publish call) instead of
    // stranding focus on a plain Inventory row -- unless the mag is now out
    // of feed charges, in which case land back on its own context menu
    // (Feed shows there, disabled) rather than silently reopening a picker
    // that would just reject the next selection.
    if (fed_this_refresh)
    {
        if (message.mag && message.mag->feed_charges_used < message.mag->feed_charges)
            BeginAwaitingMagFood();
        else
            m_reopen_mag_context_menu_pending = true;
    }
}

void HudLayer::OnCharacterScreenClosed(const CharacterScreenClosedMessage& /*message*/)
{
    if (!m_document)
        return;

    if (Rml::Element* overlay = m_document->GetElementById("character-screen"))
        overlay->SetProperty("display", "none");

    m_character_screen_listeners.clear();
    m_character_screen_hover_listeners.clear();
    m_hovered_inventory_index.reset();
    m_hovered_equipment_index.reset();
    m_requested_preview_inventory_index.reset();
    m_requested_preview_equipment_slot.reset();
    m_stat_preview.reset();
    CloseContextMenu();
    CancelAwaitingHotbarSlot();
    CancelAwaitingMagFood();
    m_character_screen_cache.reset();
    m_focused_panel = CharacterScreenPanel::Stats;
    m_focused_row = 0;
    m_mag_panel_elements.reset();
    m_mag_stat_animations = {};
}

void HudLayer::OnActionPaletteState(const ActionPaletteMessage& message)
{
    if (!m_document)
        return;

    const bool fresh_open = !m_action_palette_cache.has_value();
    m_action_palette_cache = message;
    CancelAwaitingHotbarSlot();

    if (Rml::Element* overlay = m_document->GetElementById("action-palette"))
        overlay->SetProperty("display", "flex");

    m_action_palette_listeners.clear();

    if (Rml::Element* list = m_document->GetElementById("action-palette-techniques"))
    {
        std::string markup;
        if (message.techniques.empty())
        {
            markup = "<div class=\"list-empty\">No Techniques learned yet.</div>";
        }
        else
        {
            for (const ActionPaletteMessage::TechniqueEntry& entry : message.techniques)
            {
                markup += "<div class=\"technique-row\">";
                if (!entry.icon_path.empty())
                    markup += "<img class=\"tech-icon\" src=\"" + EscapeRml(entry.icon_path) + "\"/>";
                markup += "<span class=\"tech-name\">" + EscapeRml(entry.display_name) + " (Tier " +
                          std::to_string(entry.tier) + ", " + std::to_string(entry.tp_cost) + " TP)</span></div>";
            }
        }
        list->SetInnerRML(markup);

        Rml::ElementList rows;
        list->QuerySelectorAll(rows, ".technique-row");
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const int index = static_cast<int>(i);
            auto listener = std::make_unique<RmlClickListener>(
                [this, index]()
                {
                    m_tech_focused_panel = ActionPalettePanel::Techniques;
                    m_tech_focused_row = index;
                    ActivateFocusedTechRow();
                });
            listener->Attach(*rows[i]);
            m_action_palette_listeners.push_back(std::move(listener));
        }
    }

    if (Rml::Element* list = m_document->GetElementById("action-palette-photon-arts"))
    {
        std::string markup;
        if (message.photon_arts.empty())
        {
            markup = "<div class=\"list-empty\">No Photon Arts granted by the equipped weapon.</div>";
        }
        else
        {
            for (const ActionPaletteMessage::PhotonArtEntry& entry : message.photon_arts)
                markup += "<div class=\"photon-art-row\">" + EscapeRml(entry.display_name) + " (" +
                          std::to_string(entry.tp_cost) + " TP)</div>";
        }
        list->SetInnerRML(markup);

        Rml::ElementList rows;
        list->QuerySelectorAll(rows, ".photon-art-row");
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const int index = static_cast<int>(i);
            auto listener = std::make_unique<RmlClickListener>(
                [this, index]()
                {
                    m_tech_focused_panel = ActionPalettePanel::PhotonArts;
                    m_tech_focused_row = index;
                    ActivateFocusedTechRow();
                });
            listener->Attach(*rows[i]);
            m_action_palette_listeners.push_back(std::move(listener));
        }
    }

    if (Rml::Element* list = m_document->GetElementById("action-palette-normal-attack"))
    {
        std::string markup;
        if (!message.normal_attack.has_value())
        {
            markup = "<div class=\"list-empty\">No weapon equipped.</div>";
        }
        else
        {
            markup =
                "<div class=\"normal-attack-row\">" + EscapeRml(message.normal_attack->display_name) + "</div>";
        }
        list->SetInnerRML(markup);

        Rml::ElementList rows;
        list->QuerySelectorAll(rows, ".normal-attack-row");
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const int index = static_cast<int>(i);
            auto listener = std::make_unique<RmlClickListener>(
                [this, index]()
                {
                    m_tech_focused_panel = ActionPalettePanel::NormalAttack;
                    m_tech_focused_row = index;
                    ActivateFocusedTechRow();
                });
            listener->Attach(*rows[i]);
            m_action_palette_listeners.push_back(std::move(listener));
        }
    }

    if (Rml::Element* list = m_document->GetElementById("action-palette-special-attack"))
    {
        std::string markup;
        if (!message.special_attack.has_value())
        {
            markup = "<div class=\"list-empty\">No elemental special on the equipped weapon.</div>";
        }
        else
        {
            markup =
                "<div class=\"special-attack-row\">" + EscapeRml(message.special_attack->display_name) + "</div>";
        }
        list->SetInnerRML(markup);

        Rml::ElementList rows;
        list->QuerySelectorAll(rows, ".special-attack-row");
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const int index = static_cast<int>(i);
            auto listener = std::make_unique<RmlClickListener>(
                [this, index]()
                {
                    m_tech_focused_panel = ActionPalettePanel::SpecialAttack;
                    m_tech_focused_row = index;
                    ActivateFocusedTechRow();
                });
            listener->Attach(*rows[i]);
            m_action_palette_listeners.push_back(std::move(listener));
        }
    }

    if (fresh_open)
    {
        m_tech_focused_panel = ActionPalettePanel::Techniques;
        m_tech_focused_row = 0;
    }
    else
    {
        m_tech_focused_row =
            std::clamp(m_tech_focused_row, 0, std::max(0, ActionPaletteRowCount(m_tech_focused_panel) - 1));
    }
    RenderTechniquesFocusHighlights();
}

void HudLayer::OnActionPaletteClosed(const ActionPaletteClosedMessage& /*message*/)
{
    if (!m_document)
        return;

    if (Rml::Element* overlay = m_document->GetElementById("action-palette"))
        overlay->SetProperty("display", "none");

    m_action_palette_listeners.clear();
    CancelAwaitingHotbarSlot();
    m_action_palette_cache.reset();
    m_tech_focused_panel = ActionPalettePanel::Techniques;
    m_tech_focused_row = 0;
}

int HudLayer::ActionPaletteRowCount(ActionPalettePanel panel) const
{
    if (!m_action_palette_cache)
        return 0;

    switch (panel)
    {
    case ActionPalettePanel::Techniques:
        return static_cast<int>(m_action_palette_cache->techniques.size());
    case ActionPalettePanel::PhotonArts:
        return static_cast<int>(m_action_palette_cache->photon_arts.size());
    case ActionPalettePanel::NormalAttack:
        return m_action_palette_cache->normal_attack.has_value() ? 1 : 0;
    case ActionPalettePanel::SpecialAttack:
        return m_action_palette_cache->special_attack.has_value() ? 1 : 0;
    }
    return 0;
}

void HudLayer::MoveTechPanelFocus(int direction)
{
    constexpr int kPanelCount = 4;
    const int next = std::clamp(static_cast<int>(m_tech_focused_panel) + direction, 0, kPanelCount - 1);
    m_tech_focused_panel = static_cast<ActionPalettePanel>(next);
    m_tech_focused_row =
        std::clamp(m_tech_focused_row, 0, std::max(0, ActionPaletteRowCount(m_tech_focused_panel) - 1));
    RenderTechniquesFocusHighlights();
}

void HudLayer::MoveTechRowFocus(int direction)
{
    const int count = ActionPaletteRowCount(m_tech_focused_panel);
    if (count <= 0)
        return;

    m_tech_focused_row = std::clamp(m_tech_focused_row + direction, 0, count - 1);
    RenderTechniquesFocusHighlights();
}

void HudLayer::ActivateFocusedTechRow()
{
    if (!m_action_palette_cache)
        return;

    switch (m_tech_focused_panel)
    {
    case ActionPalettePanel::Techniques:
    {
        if (m_tech_focused_row < 0 ||
            m_tech_focused_row >= static_cast<int>(m_action_palette_cache->techniques.size()))
            return;
        const ActionPaletteMessage::TechniqueEntry& entry =
            m_action_palette_cache->techniques[static_cast<std::size_t>(m_tech_focused_row)];
        BeginAwaitingAbilityHotbarSlot(HotbarSlotType::Technique, entry.technique_id);
        return;
    }
    case ActionPalettePanel::PhotonArts:
    {
        if (m_tech_focused_row < 0 ||
            m_tech_focused_row >= static_cast<int>(m_action_palette_cache->photon_arts.size()))
            return;
        const ActionPaletteMessage::PhotonArtEntry& entry =
            m_action_palette_cache->photon_arts[static_cast<std::size_t>(m_tech_focused_row)];
        BeginAwaitingAbilityHotbarSlot(HotbarSlotType::PhotonArt, entry.photon_art_id);
        return;
    }
    case ActionPalettePanel::NormalAttack:
        if (!m_action_palette_cache->normal_attack.has_value())
            return;
        BeginAwaitingAbilityHotbarSlot(HotbarSlotType::NormalAttack, 0);
        return;
    case ActionPalettePanel::SpecialAttack:
        if (!m_action_palette_cache->special_attack.has_value())
            return;
        BeginAwaitingAbilityHotbarSlot(HotbarSlotType::SpecialAttack, 0);
        return;
    }
}

void HudLayer::RenderTechniquesFocusHighlights()
{
    if (!m_document)
        return;

    RenderTechRowFocus("action-palette-techniques", ".technique-row", ActionPalettePanel::Techniques);
    RenderTechRowFocus("action-palette-photon-arts", ".photon-art-row", ActionPalettePanel::PhotonArts);
    RenderTechRowFocus("action-palette-normal-attack", ".normal-attack-row", ActionPalettePanel::NormalAttack);
    RenderTechRowFocus("action-palette-special-attack", ".special-attack-row", ActionPalettePanel::SpecialAttack);
}

void HudLayer::RenderTechRowFocus(const char* container_id, const char* row_class, ActionPalettePanel panel)
{
    Rml::Element* container = m_document->GetElementById(container_id);
    if (!container)
        return;

    container->SetClass("focused", panel == m_tech_focused_panel);

    Rml::ElementList rows;
    container->QuerySelectorAll(rows, row_class);
    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i]->SetClass("focused", panel == m_tech_focused_panel && static_cast<int>(i) == m_tech_focused_row);
}

void HudLayer::OnMissionSelectState(const MissionSelectMessage& message)
{
    if (!m_document)
        return;

    const bool fresh_open = !m_mission_select_cache.has_value();
    m_mission_select_cache = message;

    if (Rml::Element* overlay = m_document->GetElementById("mission-select-screen"))
        overlay->SetProperty("display", "flex");

    m_mission_select_listeners.clear();

    if (Rml::Element* list = m_document->GetElementById("mission-select-list"))
    {
        std::string markup;
        if (message.entries.empty())
        {
            markup = "<div class=\"list-empty\">No missions available.</div>";
        }
        else
        {
            for (const MissionSelectMessage::Entry& entry : message.entries)
                markup += std::string("<div class=\"mission-row") + (entry.unlocked ? "" : " locked") + "\">" +
                          EscapeRml(entry.name) + "</div>";
        }
        list->SetInnerRML(markup);

        Rml::ElementList rows;
        list->QuerySelectorAll(rows, ".mission-row");
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            if (i >= message.entries.size() || !message.entries[i].unlocked)
                continue;
            const int index = static_cast<int>(i);
            auto listener = std::make_unique<RmlClickListener>(
                [this, index]()
                {
                    m_mission_select_focused_row = index;
                    ActivateFocusedMissionSelectRow();
                });
            listener->Attach(*rows[i]);
            m_mission_select_listeners.push_back(std::move(listener));
        }
    }

    if (fresh_open)
        m_mission_select_focused_row = 0;
    else
        m_mission_select_focused_row = std::clamp(m_mission_select_focused_row, 0, std::max(0, MissionSelectRowCount() - 1));
    RenderMissionSelectFocusHighlight();
}

void HudLayer::OnMissionSelectClosed(const MissionSelectClosedMessage& /*message*/)
{
    if (!m_document)
        return;

    if (Rml::Element* overlay = m_document->GetElementById("mission-select-screen"))
        overlay->SetProperty("display", "none");

    m_mission_select_listeners.clear();
    m_mission_select_cache.reset();
    m_mission_select_focused_row = 0;
}

int HudLayer::MissionSelectRowCount() const
{
    return m_mission_select_cache ? static_cast<int>(m_mission_select_cache->entries.size()) : 0;
}

void HudLayer::MoveMissionSelectRowFocus(int direction)
{
    const int count = MissionSelectRowCount();
    if (count <= 0)
        return;
    m_mission_select_focused_row = std::clamp(m_mission_select_focused_row + direction, 0, count - 1);
    RenderMissionSelectFocusHighlight();
}

void HudLayer::ActivateFocusedMissionSelectRow()
{
    if (!m_mission_select_cache || m_mission_select_focused_row < 0 ||
        m_mission_select_focused_row >= static_cast<int>(m_mission_select_cache->entries.size()))
        return;

    const MissionSelectMessage::Entry& entry =
        m_mission_select_cache->entries[static_cast<std::size_t>(m_mission_select_focused_row)];
    if (!entry.unlocked)
        return;
    Publish(MissionSelectedMessage{entry.dungeon_id_string});
}

void HudLayer::RenderMissionSelectFocusHighlight()
{
    if (!m_document)
        return;

    Rml::Element* list = m_document->GetElementById("mission-select-list");
    if (!list)
        return;

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".mission-row");
    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i]->SetClass("focused", static_cast<int>(i) == m_mission_select_focused_row);
}

void HudLayer::OnPauseMenuState(const PauseMenuMessage& message)
{
    if (!m_document)
        return;

    const bool fresh_open = !m_pause_cache.has_value();
    m_pause_cache = message;

    if (Rml::Element* overlay = m_document->GetElementById("pause-screen"))
        overlay->SetProperty("display", "flex");

    if (fresh_open)
    {
        m_pause_focused_row = 0;
        m_pause_placeholder_open = false;
        if (Rml::Element* placeholder = m_document->GetElementById("pause-placeholder-panel"))
            placeholder->SetProperty("display", "none");
        if (Rml::Element* menu = m_document->GetElementById("pause-menu-panel"))
            menu->SetProperty("display", "flex");
    }
    RenderPauseFocusHighlight();
}

void HudLayer::OnPauseMenuClosed(const PauseMenuClosedMessage& /*message*/)
{
    if (!m_document)
        return;

    if (Rml::Element* overlay = m_document->GetElementById("pause-screen"))
        overlay->SetProperty("display", "none");

    m_pause_cache.reset();
    m_pause_focused_row = 0;
    m_pause_placeholder_open = false;
}

void HudLayer::MovePauseRowFocus(int direction)
{
    constexpr int kPauseRowCount = 5;
    m_pause_focused_row = std::clamp(m_pause_focused_row + direction, 0, kPauseRowCount - 1);
    RenderPauseFocusHighlight();
}

void HudLayer::ActivateFocusedPauseRow()
{
    if (!m_pause_cache)
        return;

    switch (m_pause_focused_row)
    {
    case 0:
        Publish(PauseMenuActionMessage{PauseMenuAction::Resume});
        break;
    case 1:
        ShowPausePlaceholder("Options");
        break;
    case 2:
        ShowPausePlaceholder("Help");
        break;
    case 3:
        Publish(PauseMenuActionMessage{PauseMenuAction::QuitToTitle});
        break;
    case 4:
        Publish(PauseMenuActionMessage{PauseMenuAction::QuitToDesktop});
        break;
    default:
        break;
    }
}

void HudLayer::RenderPauseFocusHighlight()
{
    if (!m_document)
        return;

    Rml::Element* list = m_document->GetElementById("pause-screen-list");
    if (!list)
        return;

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".mission-row");
    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i]->SetClass("focused", static_cast<int>(i) == m_pause_focused_row);
}

void HudLayer::ShowPausePlaceholder(const char* title)
{
    if (!m_document)
        return;
    if (Rml::Element* menu = m_document->GetElementById("pause-menu-panel"))
        menu->SetProperty("display", "none");
    if (Rml::Element* placeholder_title = m_document->GetElementById("pause-placeholder-title"))
        placeholder_title->SetInnerRML(title);
    if (Rml::Element* placeholder = m_document->GetElementById("pause-placeholder-panel"))
        placeholder->SetProperty("display", "flex");
    m_pause_placeholder_open = true;
}

void HudLayer::HidePausePlaceholder()
{
    if (!m_document)
        return;
    if (Rml::Element* placeholder = m_document->GetElementById("pause-placeholder-panel"))
        placeholder->SetProperty("display", "none");
    if (Rml::Element* menu = m_document->GetElementById("pause-menu-panel"))
        menu->SetProperty("display", "flex");
    m_pause_placeholder_open = false;
}

void HudLayer::OnConfirmState(const ConfirmMessage& message)
{
    if (!m_document)
        return;

    const bool fresh_open = !m_confirm_cache.has_value();
    m_confirm_cache = message;

    if (Rml::Element* overlay = m_document->GetElementById("confirm-dialog"))
        overlay->SetProperty("display", "flex");
    if (Rml::Element* text = m_document->GetElementById("confirm-dialog-message"))
        text->SetInnerRML(EscapeRml(message.text));

    if (fresh_open)
        m_confirm_focused_row = 1; // defaults to No -- see the class doc comment
    RenderConfirmFocusHighlight();
}

void HudLayer::OnConfirmClosed(const ConfirmClosedMessage& /*message*/)
{
    if (!m_document)
        return;

    if (Rml::Element* overlay = m_document->GetElementById("confirm-dialog"))
        overlay->SetProperty("display", "none");

    m_confirm_cache.reset();
    m_confirm_focused_row = 1;
}

void HudLayer::MoveConfirmRowFocus(int direction)
{
    constexpr int kConfirmRowCount = 2;
    m_confirm_focused_row = std::clamp(m_confirm_focused_row + direction, 0, kConfirmRowCount - 1);
    RenderConfirmFocusHighlight();
}

void HudLayer::ActivateFocusedConfirmRow()
{
    if (!m_confirm_cache)
        return;
    Publish(ConfirmChoiceMessage{m_confirm_focused_row == 0});
}

void HudLayer::RenderConfirmFocusHighlight()
{
    if (!m_document)
        return;

    Rml::Element* list = m_document->GetElementById("confirm-dialog-list");
    if (!list)
        return;

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".mission-row");
    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i]->SetClass("focused", static_cast<int>(i) == m_confirm_focused_row);
}

void HudLayer::OnReturnedToTitle(const ReturnedToTitleMessage& /*message*/) { RemoveSelf(); }

void HudLayer::OnShopScreenState(const ShopMessage& message)
{
    if (!m_document)
        return;

    const bool fresh_open = !m_shop_cache.has_value();
    m_shop_cache = message;

    if (Rml::Element* overlay = m_document->GetElementById("shop-screen"))
        overlay->SetProperty("display", "flex");

    m_shop_listeners.clear();

    if (Rml::Element* meseta = m_document->GetElementById("shop-meseta"))
        meseta->SetInnerRML("Meseta: " + std::to_string(message.current_meseta));

    if (Rml::Element* list = m_document->GetElementById("shop-stock-list"))
    {
        std::string markup;
        if (message.stock.empty())
        {
            markup = "<div class=\"list-empty\">No stock.</div>";
        }
        else
        {
            for (const ShopMessage::StockEntry& entry : message.stock)
                markup += std::string("<div class=\"shop-stock-row") + (entry.affordable ? "" : " unaffordable") +
                          "\">" + EscapeRml(entry.display_name) + " (" + std::to_string(entry.buy_price) +
                          ")</div>";
        }
        list->SetInnerRML(markup);

        Rml::ElementList rows;
        list->QuerySelectorAll(rows, ".shop-stock-row");
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const int index = static_cast<int>(i);
            auto listener = std::make_unique<RmlClickListener>(
                [this, index]()
                {
                    m_shop_focused_panel = ShopScreenPanel::Stock;
                    m_shop_focused_row = index;
                    ActivateFocusedShopRow();
                });
            listener->Attach(*rows[i]);
            m_shop_listeners.push_back(std::move(listener));
        }
    }

    if (Rml::Element* list = m_document->GetElementById("shop-sellable-list"))
    {
        std::string markup;
        if (message.sellable.empty())
        {
            markup = "<div class=\"list-empty\">Nothing to sell.</div>";
        }
        else
        {
            for (const ShopMessage::SellEntry& entry : message.sellable)
                markup += "<div class=\"shop-sell-row\">" + EscapeRml(entry.display_name) + " (" +
                          std::to_string(entry.sell_value) + ")</div>";
        }
        list->SetInnerRML(markup);

        Rml::ElementList rows;
        list->QuerySelectorAll(rows, ".shop-sell-row");
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const int index = static_cast<int>(i);
            auto listener = std::make_unique<RmlClickListener>(
                [this, index]()
                {
                    m_shop_focused_panel = ShopScreenPanel::Sellable;
                    m_shop_focused_row = index;
                    ActivateFocusedShopRow();
                });
            listener->Attach(*rows[i]);
            m_shop_listeners.push_back(std::move(listener));
        }
    }

    if (fresh_open)
    {
        m_shop_focused_panel = ShopScreenPanel::Stock;
        m_shop_focused_row = 0;
    }
    else
    {
        m_shop_focused_row = std::clamp(m_shop_focused_row, 0, std::max(0, ShopScreenRowCount(m_shop_focused_panel) - 1));
    }
    RenderShopFocusHighlights();
}

void HudLayer::OnShopScreenClosed(const ShopClosedMessage& /*message*/)
{
    if (!m_document)
        return;

    if (Rml::Element* overlay = m_document->GetElementById("shop-screen"))
        overlay->SetProperty("display", "none");

    m_shop_listeners.clear();
    m_shop_cache.reset();
    m_shop_focused_panel = ShopScreenPanel::Stock;
    m_shop_focused_row = 0;
}

int HudLayer::ShopScreenRowCount(ShopScreenPanel panel) const
{
    if (!m_shop_cache)
        return 0;
    return panel == ShopScreenPanel::Stock ? static_cast<int>(m_shop_cache->stock.size())
                                           : static_cast<int>(m_shop_cache->sellable.size());
}

void HudLayer::MoveShopPanelFocus(int direction)
{
    constexpr int kPanelCount = 2;
    const int next = std::clamp(static_cast<int>(m_shop_focused_panel) + direction, 0, kPanelCount - 1);
    m_shop_focused_panel = static_cast<ShopScreenPanel>(next);
    m_shop_focused_row = std::clamp(m_shop_focused_row, 0, std::max(0, ShopScreenRowCount(m_shop_focused_panel) - 1));
    RenderShopFocusHighlights();
}

void HudLayer::MoveShopRowFocus(int direction)
{
    const int count = ShopScreenRowCount(m_shop_focused_panel);
    if (count <= 0)
        return;
    m_shop_focused_row = std::clamp(m_shop_focused_row + direction, 0, count - 1);
    RenderShopFocusHighlights();
}

void HudLayer::ActivateFocusedShopRow()
{
    if (!m_shop_cache)
        return;

    if (m_shop_focused_panel == ShopScreenPanel::Stock)
    {
        if (m_shop_focused_row < 0 || m_shop_focused_row >= static_cast<int>(m_shop_cache->stock.size()))
            return;
        if (!m_shop_cache->stock[static_cast<std::size_t>(m_shop_focused_row)].affordable)
            return;
        Publish(ShopBuyRequestedMessage{m_shop_focused_row});
    }
    else
    {
        if (m_shop_focused_row < 0 || m_shop_focused_row >= static_cast<int>(m_shop_cache->sellable.size()))
            return;
        Publish(ShopSellRequestedMessage{m_shop_focused_row});
    }
}

void HudLayer::RenderShopFocusHighlights()
{
    if (!m_document)
        return;

    RenderShopRowFocus("shop-stock-list", ".shop-stock-row", ShopScreenPanel::Stock);
    RenderShopRowFocus("shop-sellable-list", ".shop-sell-row", ShopScreenPanel::Sellable);
}

void HudLayer::RenderShopRowFocus(const char* container_id, const char* row_class, ShopScreenPanel panel)
{
    Rml::Element* container = m_document->GetElementById(container_id);
    if (!container)
        return;

    container->SetClass("focused", panel == m_shop_focused_panel);

    Rml::ElementList rows;
    container->QuerySelectorAll(rows, row_class);
    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i]->SetClass("focused", panel == m_shop_focused_panel && static_cast<int>(i) == m_shop_focused_row);
}

void HudLayer::OnStorageScreenState(const StorageMessage& message)
{
    if (!m_document)
        return;

    const bool fresh_open = !m_storage_cache.has_value();
    m_storage_cache = message;

    if (Rml::Element* overlay = m_document->GetElementById("storage-screen"))
        overlay->SetProperty("display", "flex");

    m_storage_listeners.clear();

    if (Rml::Element* list = m_document->GetElementById("storage-inventory-list"))
    {
        std::string markup;
        if (message.inventory.empty())
        {
            markup = "<div class=\"list-empty\">Inventory empty.</div>";
        }
        else
        {
            for (const CharacterScreenMessage::ItemEntry& entry : message.inventory)
                markup += "<div class=\"storage-inventory-row\">" + EscapeRml(ItemRowLabel(entry)) + "</div>";
        }
        list->SetInnerRML(markup);

        Rml::ElementList rows;
        list->QuerySelectorAll(rows, ".storage-inventory-row");
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const int index = static_cast<int>(i);
            auto listener = std::make_unique<RmlClickListener>(
                [this, index]()
                {
                    m_storage_focused_panel = StorageScreenPanel::Inventory;
                    m_storage_focused_row = index;
                    ActivateFocusedStorageRow();
                });
            listener->Attach(*rows[i]);
            m_storage_listeners.push_back(std::move(listener));
        }
    }

    if (Rml::Element* list = m_document->GetElementById("storage-storage-list"))
    {
        std::string markup;
        if (message.storage.empty())
        {
            markup = "<div class=\"list-empty\">Storage empty.</div>";
        }
        else
        {
            for (const CharacterScreenMessage::ItemEntry& entry : message.storage)
                markup += "<div class=\"storage-storage-row\">" + EscapeRml(ItemRowLabel(entry)) + "</div>";
        }
        list->SetInnerRML(markup);

        Rml::ElementList rows;
        list->QuerySelectorAll(rows, ".storage-storage-row");
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const int index = static_cast<int>(i);
            auto listener = std::make_unique<RmlClickListener>(
                [this, index]()
                {
                    m_storage_focused_panel = StorageScreenPanel::Storage;
                    m_storage_focused_row = index;
                    ActivateFocusedStorageRow();
                });
            listener->Attach(*rows[i]);
            m_storage_listeners.push_back(std::move(listener));
        }
    }

    if (fresh_open)
    {
        m_storage_focused_panel = StorageScreenPanel::Inventory;
        m_storage_focused_row = 0;
    }
    else
    {
        m_storage_focused_row =
            std::clamp(m_storage_focused_row, 0, std::max(0, StorageScreenRowCount(m_storage_focused_panel) - 1));
    }
    RenderStorageFocusHighlights();
}

void HudLayer::OnStorageScreenClosed(const StorageClosedMessage& /*message*/)
{
    if (!m_document)
        return;

    if (Rml::Element* overlay = m_document->GetElementById("storage-screen"))
        overlay->SetProperty("display", "none");

    m_storage_listeners.clear();
    m_storage_cache.reset();
    m_storage_focused_panel = StorageScreenPanel::Inventory;
    m_storage_focused_row = 0;
}

int HudLayer::StorageScreenRowCount(StorageScreenPanel panel) const
{
    if (!m_storage_cache)
        return 0;
    return panel == StorageScreenPanel::Inventory ? static_cast<int>(m_storage_cache->inventory.size())
                                                  : static_cast<int>(m_storage_cache->storage.size());
}

void HudLayer::MoveStoragePanelFocus(int direction)
{
    constexpr int kPanelCount = 2;
    const int next = std::clamp(static_cast<int>(m_storage_focused_panel) + direction, 0, kPanelCount - 1);
    m_storage_focused_panel = static_cast<StorageScreenPanel>(next);
    m_storage_focused_row =
        std::clamp(m_storage_focused_row, 0, std::max(0, StorageScreenRowCount(m_storage_focused_panel) - 1));
    RenderStorageFocusHighlights();
}

void HudLayer::MoveStorageRowFocus(int direction)
{
    const int count = StorageScreenRowCount(m_storage_focused_panel);
    if (count <= 0)
        return;
    m_storage_focused_row = std::clamp(m_storage_focused_row + direction, 0, count - 1);
    RenderStorageFocusHighlights();
}

void HudLayer::ActivateFocusedStorageRow()
{
    if (!m_storage_cache)
        return;

    if (m_storage_focused_panel == StorageScreenPanel::Inventory)
    {
        if (m_storage_focused_row < 0 || m_storage_focused_row >= static_cast<int>(m_storage_cache->inventory.size()))
            return;
        Publish(StorageItemActivatedMessage{m_storage_focused_row});
    }
    else
    {
        if (m_storage_focused_row < 0 || m_storage_focused_row >= static_cast<int>(m_storage_cache->storage.size()))
            return;
        Publish(StorageWithdrawRequestedMessage{m_storage_focused_row});
    }
}

void HudLayer::RenderStorageFocusHighlights()
{
    if (!m_document)
        return;

    RenderStorageRowFocus("storage-inventory-list", ".storage-inventory-row", StorageScreenPanel::Inventory);
    RenderStorageRowFocus("storage-storage-list", ".storage-storage-row", StorageScreenPanel::Storage);
}

void HudLayer::RenderStorageRowFocus(const char* container_id, const char* row_class, StorageScreenPanel panel)
{
    Rml::Element* container = m_document->GetElementById(container_id);
    if (!container)
        return;

    container->SetClass("focused", panel == m_storage_focused_panel);

    Rml::ElementList rows;
    container->QuerySelectorAll(rows, row_class);
    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i]->SetClass("focused", panel == m_storage_focused_panel && static_cast<int>(i) == m_storage_focused_row);
}

void HudLayer::RenderStatsPanel()
{
    Rml::Element* panel = m_document->GetElementById("character-screen-stats");
    if (!panel || !m_character_screen_cache)
        return;

    const CharacterScreenMessage::StatsSummary& stats = m_character_screen_cache->stats;
    const bool preview_active = m_stat_preview && m_stat_preview->active;

    std::string markup;
    markup += "<div class=\"stat-row character-name\">Player</div>";
    markup += "<div class=\"stat-row\">Lv: " + std::to_string(stats.level) + "</div>";
    markup += "<div class=\"stat-row\">XP to Next: " + std::to_string(stats.xp) + " / " +
              std::to_string(stats.xp_to_next) + "</div>";
    markup += "<div class=\"stat-row\">Total EXP: " + std::to_string(stats.total_xp) + "</div>";
    markup += "<div class=\"stats-separator\"></div>";
    markup +=
        "<div class=\"stat-row\">HP: " + std::to_string(stats.hp) + " / " + std::to_string(stats.max_hp) + "</div>";
    markup +=
        "<div class=\"stat-row\">TP: " + std::to_string(stats.tp) + " / " + std::to_string(stats.max_tp) + "</div>";
    markup += StatRow("ATP", stats.atp, preview_active ? m_stat_preview->atp_delta : 0);
    markup += StatRow("ATA", stats.ata, preview_active ? m_stat_preview->ata_delta : 0);
    markup += StatRow("MST", stats.mst, preview_active ? m_stat_preview->mst_delta : 0);
    markup += StatRow("DFP", stats.dfp, preview_active ? m_stat_preview->dfp_delta : 0);
    markup += StatRow("EVP", stats.evp, preview_active ? m_stat_preview->evp_delta : 0);
    markup += StatRow("LCK", stats.lck, preview_active ? m_stat_preview->lck_delta : 0);
    panel->SetInnerRML(markup);
}

int HudLayer::CharacterScreenRowCount(CharacterScreenPanel panel) const
{
    if (!m_character_screen_cache)
        return 0;

    switch (panel)
    {
    case CharacterScreenPanel::Stats:
        return 0;
    case CharacterScreenPanel::Equipment:
        return static_cast<int>(m_character_screen_cache->equipment.size());
    case CharacterScreenPanel::Inventory:
        return static_cast<int>(m_character_screen_cache->inventory.size());
    }
    return 0; // unreachable for a valid enum value
}

void HudLayer::MovePanelFocus(int direction)
{
    constexpr int kPanelCount = 3;
    const int next = std::clamp(static_cast<int>(m_focused_panel) + direction, 0, kPanelCount - 1);
    m_focused_panel = static_cast<CharacterScreenPanel>(next);
    m_focused_row = std::clamp(m_focused_row, 0, std::max(0, CharacterScreenRowCount(m_focused_panel) - 1));
    RenderFocusHighlights();
}

void HudLayer::MoveRowFocus(int direction)
{
    const int count = CharacterScreenRowCount(m_focused_panel);
    if (count <= 0)
        return;

    m_focused_row = std::clamp(m_focused_row + direction, 0, count - 1);
    RenderFocusHighlights();
}

void HudLayer::ActivateFocusedRow()
{
    if (m_awaiting_mag_food_selection)
    {
        if (m_focused_panel == CharacterScreenPanel::Inventory && m_character_screen_cache && m_focused_row >= 0 &&
            m_focused_row < static_cast<int>(m_character_screen_cache->inventory.size()) &&
            m_character_screen_cache->inventory[static_cast<std::size_t>(m_focused_row)].is_mag_food)
        {
            const int index = m_focused_row;
            CancelAwaitingMagFood();
            Publish(MagFeedRequestedMessage{index});
        }
        return;
    }

    if (m_focused_panel == CharacterScreenPanel::Stats)
        return;

    OpenContextMenu(m_focused_panel, m_focused_row);
}

void HudLayer::OpenContextMenu(CharacterScreenPanel panel, int index)
{
    if (!m_document || panel == CharacterScreenPanel::Stats)
        return;
    if (index < 0 || index >= CharacterScreenRowCount(panel))
        return;

    CancelAwaitingHotbarSlot();
    CancelAwaitingMagFood();

    m_focused_panel = panel;
    m_focused_row = index;

    m_menu_options = BuildMenuOptions(panel, index);
    if (m_menu_options.empty())
    {
        RenderFocusHighlights();
        return;
    }

    m_menu_open = true;
    m_menu_panel = panel;
    m_menu_index = index;
    m_menu_highlight = 0;
    RenderContextMenu();
    RenderFocusHighlights();
}

void HudLayer::CloseContextMenu()
{
    m_menu_open = false;
    m_menu_options.clear();
    m_context_menu_listeners.clear();

    if (m_document)
        if (Rml::Element* menu = m_document->GetElementById("character-screen-context-menu"))
            menu->SetProperty("display", "none");
}

void HudLayer::MoveMenuHighlight(int direction)
{
    if (m_menu_options.empty())
        return;

    const int count = static_cast<int>(m_menu_options.size());
    m_menu_highlight = ((m_menu_highlight + direction) % count + count) % count;
    UpdateMenuHighlightClasses();
}

void HudLayer::ChooseHighlightedMenuOption()
{
    if (m_menu_highlight < 0 || m_menu_highlight >= static_cast<int>(m_menu_options.size()))
        return;

    const ContextMenuOption& option = m_menu_options[static_cast<std::size_t>(m_menu_highlight)];
    if (option.disabled)
        return;

    const ContextMenuOption::Action action = option.action;
    const CharacterScreenPanel panel = m_menu_panel;
    const int index = m_menu_index;

    if (action == ContextMenuOption::Action::AssignToHotbar)
    {
        BeginAwaitingHotbarSlot(index);
        return;
    }
    if (action == ContextMenuOption::Action::Feed)
    {
        BeginAwaitingMagFood();
        return;
    }

    CloseContextMenu();

    switch (action)
    {
    case ContextMenuOption::Action::Equip:
        if (panel == CharacterScreenPanel::Equipment)
            JumpToMatchingInventoryItem(static_cast<EquipmentSlot>(index));
        else
            Publish(InventoryItemActivatedMessage{index, InventoryItemAction::Equip});
        break;
    case ContextMenuOption::Action::Remove:
        Publish(EquipmentSlotActivatedMessage{static_cast<EquipmentSlot>(index)});
        break;
    case ContextMenuOption::Action::Use:
        Publish(InventoryItemActivatedMessage{index, InventoryItemAction::Use});
        break;
    case ContextMenuOption::Action::Drop:
        Publish(InventoryItemActivatedMessage{index, InventoryItemAction::Drop});
        break;
    case ContextMenuOption::Action::AssignToHotbar:
    case ContextMenuOption::Action::Feed:
        break; // handled above, before the menu closes
    }
}

void HudLayer::BeginAwaitingHotbarSlot(int inventory_index)
{
    CloseContextMenu();
    m_awaiting_hotbar_assign_source = HotbarAssignSource::CharacterScreenItem;
    m_awaiting_hotbar_inventory_index = inventory_index;
    SetCharacterScreenHint(kAwaitingHotbarSlotHint, /*awaiting=*/true);
}

void HudLayer::BeginAwaitingAbilityHotbarSlot(HotbarSlotType type, std::uint32_t id)
{
    m_awaiting_hotbar_assign_source = HotbarAssignSource::ActionPaletteAbility;
    m_awaiting_hotbar_ability_type = type;
    m_awaiting_hotbar_ability_id = id;
    SetActionPaletteHint(kAwaitingHotbarSlotHint, /*awaiting=*/true);
}

void HudLayer::CancelAwaitingHotbarSlot()
{
    m_awaiting_hotbar_assign_source = HotbarAssignSource::None;
    m_awaiting_hotbar_inventory_index = -1;
    m_awaiting_hotbar_ability_type = HotbarSlotType::Empty;
    m_awaiting_hotbar_ability_id = 0;
    // Both guarded rather than switched on source -- CancelAwaitingHotbarSlot
    // is also called unconditionally from screen open/close paths where
    // nothing was actually awaiting, so this just restores whichever
    // screen's hint is currently in the DOM (the other is hidden/inert).
    if (m_character_screen_cache)
        SetCharacterScreenHint(kDefaultCharacterScreenHint, /*awaiting=*/false);
    if (m_action_palette_cache)
        SetActionPaletteHint(kDefaultActionPaletteHint, /*awaiting=*/false);
}

void HudLayer::BeginAwaitingMagFood()
{
    CloseContextMenu();
    m_awaiting_mag_food_selection = true;
    m_focused_panel = CharacterScreenPanel::Inventory;
    m_focused_row = 0;

    if (m_character_screen_cache)
    {
        const std::vector<CharacterScreenMessage::ItemEntry>& inventory = m_character_screen_cache->inventory;
        for (std::size_t i = 0; i < inventory.size(); ++i)
        {
            if (inventory[i].is_mag_food)
            {
                m_focused_row = static_cast<int>(i);
                break;
            }
        }
    }

    SetCharacterScreenHint(kAwaitingMagFoodHint, /*awaiting=*/true);
    RenderFocusHighlights();
}

void HudLayer::CancelAwaitingMagFood()
{
    if (!m_awaiting_mag_food_selection)
        return;

    m_awaiting_mag_food_selection = false;
    if (m_character_screen_cache)
        SetCharacterScreenHint(kDefaultCharacterScreenHint, /*awaiting=*/false);
}

void HudLayer::SetCharacterScreenHint(const char* text, bool awaiting)
{
    if (!m_document)
        return;

    Rml::Element* hint = m_document->GetElementById("character-screen-hint");
    if (!hint)
        return;

    hint->SetInnerRML(EscapeRml(text));
    hint->SetClass("awaiting-hotbar-slot", awaiting);
}

void HudLayer::SetActionPaletteHint(const char* text, bool awaiting)
{
    if (!m_document)
        return;

    Rml::Element* hint = m_document->GetElementById("action-palette-hint");
    if (!hint)
        return;

    hint->SetInnerRML(EscapeRml(text));
    hint->SetClass("awaiting-hotbar-slot", awaiting);
}

void HudLayer::JumpToMatchingInventoryItem(EquipmentSlot slot)
{
    m_focused_panel = CharacterScreenPanel::Inventory;
    m_focused_row = 0;

    if (m_character_screen_cache)
    {
        const std::vector<CharacterScreenMessage::ItemEntry>& inventory = m_character_screen_cache->inventory;
        for (std::size_t i = 0; i < inventory.size(); ++i)
        {
            if (inventory[i].equip_slot == slot)
            {
                m_focused_row = static_cast<int>(i);
                break;
            }
        }
    }

    RenderFocusHighlights();
}

void HudLayer::UpdateStatPreview()
{
    std::optional<int> inventory_target;
    std::optional<EquipmentSlot> equipment_target;

    if (m_hovered_inventory_index)
        inventory_target = m_hovered_inventory_index;
    else if (m_hovered_equipment_index)
        equipment_target = static_cast<EquipmentSlot>(*m_hovered_equipment_index);
    else if (m_focused_panel == CharacterScreenPanel::Inventory)
        inventory_target = m_focused_row;
    else if (m_focused_panel == CharacterScreenPanel::Equipment)
        equipment_target = static_cast<EquipmentSlot>(m_focused_row);

    if (inventory_target &&
        (!m_character_screen_cache || *inventory_target < 0 ||
         *inventory_target >= static_cast<int>(m_character_screen_cache->inventory.size()) ||
         !m_character_screen_cache->inventory[static_cast<std::size_t>(*inventory_target)].equip_slot.has_value()))
        inventory_target.reset();

    if (equipment_target)
    {
        const std::size_t index = static_cast<std::size_t>(*equipment_target);
        if (!m_character_screen_cache || index >= m_character_screen_cache->equipment.size() ||
            !m_character_screen_cache->equipment[index].has_value())
            equipment_target.reset();
    }

    if (inventory_target == m_requested_preview_inventory_index &&
        equipment_target == m_requested_preview_equipment_slot)
        return;
    m_requested_preview_inventory_index = inventory_target;
    m_requested_preview_equipment_slot = equipment_target;

    // Response arrives via OnStatPreview, once GameplayLayer's own message
    // queue processes this request -- see InventoryItemHoverChangedMessage/
    // EquipmentSlotHoverChangedMessage's own doc comments.
    if (inventory_target)
        Publish(InventoryItemHoverChangedMessage{*inventory_target});
    else if (equipment_target)
        Publish(EquipmentSlotHoverChangedMessage{equipment_target});
    else
    {
        m_stat_preview.reset();
        RenderStatsPanel();
    }
}

void HudLayer::OnStatPreview(const CharacterScreenStatPreviewMessage& message)
{
    m_stat_preview = message;
    RenderStatsPanel();
}

std::vector<HudLayer::ContextMenuOption> HudLayer::BuildMenuOptions(CharacterScreenPanel panel, int index) const
{
    std::vector<ContextMenuOption> options;
    if (!m_character_screen_cache)
        return options;

    if (panel == CharacterScreenPanel::Equipment)
    {
        if (index < 0 || index >= static_cast<int>(m_character_screen_cache->equipment.size()))
            return options;

        options.push_back({ContextMenuOption::Action::Equip, "Equip"});
        if (m_character_screen_cache->equipment[static_cast<std::size_t>(index)].has_value())
        {
            options.push_back({ContextMenuOption::Action::Remove, "Remove"});
            if (static_cast<EquipmentSlot>(index) == EquipmentSlot::Mag)
            {
                const bool out_of_charges = m_character_screen_cache->mag &&
                                             m_character_screen_cache->mag->feed_charges_used >=
                                                 m_character_screen_cache->mag->feed_charges;
                options.push_back({ContextMenuOption::Action::Feed, "Feed", out_of_charges});
            }
        }
    }
    else if (panel == CharacterScreenPanel::Inventory)
    {
        if (index < 0 || index >= static_cast<int>(m_character_screen_cache->inventory.size()))
            return options;

        const CharacterScreenMessage::ItemEntry& entry =
            m_character_screen_cache->inventory[static_cast<std::size_t>(index)];
        if (entry.equip_slot.has_value())
            options.push_back({ContextMenuOption::Action::Equip, "Equip"});
        if (entry.is_consumable)
        {
            options.push_back({ContextMenuOption::Action::Use, "Use"});
            options.push_back({ContextMenuOption::Action::AssignToHotbar, "Assign to Hotbar"});
        }
        options.push_back({ContextMenuOption::Action::Drop, "Drop"});
    }

    return options;
}

void HudLayer::RenderContextMenu()
{
    Rml::Element* menu = m_document->GetElementById("character-screen-context-menu");
    Rml::Element* anchor = CharacterScreenRowElement(m_menu_panel, m_menu_index);
    Rml::Element* screen = m_document->GetElementById("character-screen");
    if (!menu || !anchor || !screen)
        return;

    m_context_menu_listeners.clear();

    std::string markup;
    for (const ContextMenuOption& option : m_menu_options)
        markup += std::string("<div class=\"menu-row") + (option.disabled ? " disabled" : "") + "\">" +
                   EscapeRml(option.label) + "</div>";
    menu->SetInnerRML(markup);
    menu->SetProperty("display", "block");

    Rml::ElementList rows;
    menu->QuerySelectorAll(rows, ".menu-row");
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        const int option_index = static_cast<int>(i);
        auto listener = std::make_unique<RmlClickListener>(
            [this, option_index]()
            {
                m_menu_highlight = option_index;
                ChooseHighlightedMenuOption();
            });
        listener->Attach(*rows[i]);
        m_context_menu_listeners.push_back(std::move(listener));
    }

    // Anchored to the right of the target row, flipped/clamped to stay
    // within #character-screen -- see kContextMenuWidth/kContextMenuMaxHeight's
    // doc comment for why fixed constants are used instead of live layout.
    const Rml::Vector2f screen_offset = screen->GetAbsoluteOffset();
    const Rml::Vector2f screen_size = screen->GetBox().GetSize();
    const Rml::Vector2f anchor_offset = anchor->GetAbsoluteOffset();
    const Rml::Vector2f anchor_size = anchor->GetBox().GetSize();

    float local_x = anchor_offset.x + anchor_size.x - screen_offset.x;
    float local_y = anchor_offset.y - screen_offset.y;

    if (local_y + kContextMenuMaxHeight > screen_size.y)
        local_y = std::max(0.0f, local_y - kContextMenuMaxHeight + anchor_size.y);
    local_x = std::clamp(local_x, 0.0f, std::max(0.0f, screen_size.x - kContextMenuWidth));
    local_y = std::clamp(local_y, 0.0f, std::max(0.0f, screen_size.y - kContextMenuMaxHeight));

    menu->SetProperty("left", std::to_string(local_x) + "px");
    menu->SetProperty("top", std::to_string(local_y) + "px");

    UpdateMenuHighlightClasses();
}

void HudLayer::UpdateMenuHighlightClasses()
{
    Rml::Element* menu = m_document->GetElementById("character-screen-context-menu");
    if (!menu)
        return;

    Rml::ElementList rows;
    menu->QuerySelectorAll(rows, ".menu-row");
    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i]->SetClass("focused", static_cast<int>(i) == m_menu_highlight);
}

void HudLayer::RenderFocusHighlights()
{
    if (!m_document)
        return;

    if (Rml::Element* stats = m_document->GetElementById("character-screen-stats"))
        stats->SetClass("focused", m_focused_panel == CharacterScreenPanel::Stats);

    RenderRowFocus("character-screen-equipment", ".equip-row", CharacterScreenPanel::Equipment);
    RenderRowFocus("character-screen-inventory", ".inventory-row", CharacterScreenPanel::Inventory);

    RenderMagPanel();
    RenderItemDetailPanel();
    UpdateStatPreview();
}

void HudLayer::RenderItemDetailPanel()
{
    if (!m_document)
        return;

    Rml::Element* panel = m_document->GetElementById("character-screen-item-detail");
    if (!panel)
        return;

    std::optional<int> inventory_target;
    std::optional<EquipmentSlot> equipment_target;

    if (m_hovered_inventory_index)
        inventory_target = m_hovered_inventory_index;
    else if (m_hovered_equipment_index)
        equipment_target = static_cast<EquipmentSlot>(*m_hovered_equipment_index);
    else if (m_focused_panel == CharacterScreenPanel::Inventory)
        inventory_target = m_focused_row;
    else if (m_focused_panel == CharacterScreenPanel::Equipment)
        equipment_target = static_cast<EquipmentSlot>(m_focused_row);

    const CharacterScreenMessage::ItemEntry* entry = nullptr;
    if (m_character_screen_cache)
    {
        if (inventory_target && *inventory_target >= 0 &&
            *inventory_target < static_cast<int>(m_character_screen_cache->inventory.size()))
            entry = &m_character_screen_cache->inventory[static_cast<std::size_t>(*inventory_target)];
        else if (equipment_target)
        {
            const std::size_t index = static_cast<std::size_t>(*equipment_target);
            if (index < m_character_screen_cache->equipment.size() &&
                m_character_screen_cache->equipment[index].has_value())
                entry = &*m_character_screen_cache->equipment[index];
        }
    }

    if (!entry)
    {
        panel->SetProperty("display", "none");
        return;
    }

    std::string markup = "<div class=\"item-detail-title\">" + EscapeRml(entry->display_name);
    if (entry->rarity_stars > 0)
        markup += " <span class=\"item-detail-stars\">" + StarsMarkup(entry->rarity_stars) + "</span>";
    markup += "</div>";
    markup += "<div class=\"item-detail-columns\">";
    markup += "<div class=\"item-detail-image\"><span class=\"item-detail-image-placeholder\">?</span></div>";
    markup += "<div class=\"item-detail-info\">";

    if (!entry->description.empty())
    {
        markup += "<div class=\"item-detail-description\">" + EscapeRml(entry->description) + "</div>";
        markup += "<div class=\"item-detail-separator\"></div>";
    }

    if (entry->equip_slot)
        markup += "<div class=\"item-detail-stat-row\">Equip Slot: " +
                  std::string(kEquipSlotLabels[static_cast<std::size_t>(*entry->equip_slot)]) + "</div>";

    if (entry->stats)
    {
        const std::array<std::pair<const char*, int>, 6> stat_rows = {
            {{"ATP", entry->stats->atp}, {"ATA", entry->stats->ata}, {"MST", entry->stats->mst},
             {"DFP", entry->stats->dfp}, {"EVP", entry->stats->evp}, {"LCK", entry->stats->lck}}};
        for (const auto& [label, value] : stat_rows)
            if (value != 0)
                markup += std::string("<div class=\"item-detail-stat-row\">") + label + ": " + std::to_string(value) +
                          "</div>";
    }

    for (const auto& [race_name, bonus_percent] : entry->species_bonuses)
        markup += "<div class=\"item-detail-stat-row\">vs " + EscapeRml(race_name) + ": +" +
                  std::to_string(bonus_percent) + "%</div>";

    if (entry->weapon_detail)
    {
        const CharacterScreenMessage::ItemEntry::WeaponDetail& weapon_detail = *entry->weapon_detail;
        markup += std::string("<div class=\"item-detail-stat-row\">") +
                  (weapon_detail.fires_projectile ? "Ranged Weapon" : "Melee Weapon") + "</div>";
        markup += "<div class=\"item-detail-stat-row\">Range: " +
                  RangeShapeLabel(weapon_detail.range_shape, weapon_detail.range, weapon_detail.hits_per_turn) +
                  "</div>";
        markup += std::string("<div class=\"item-detail-stat-row\">Targeting: ") +
                  TargetingModeLabel(weapon_detail.targeting_mode) + "</div>";
        if (weapon_detail.max_grind_level > 0)
            markup += "<div class=\"item-detail-stat-row\">Grind: +" + std::to_string(weapon_detail.grind_level) +
                      " (max +" + std::to_string(weapon_detail.max_grind_level) + ")</div>";
        if (!weapon_detail.status_effect_name.empty())
            markup += "<div class=\"item-detail-stat-row\">" + std::to_string(weapon_detail.status_chance_percent) +
                      "% chance: " + EscapeRml(weapon_detail.status_effect_name) + "</div>";
        for (const std::string& photon_art_name : weapon_detail.photon_art_names)
            markup += "<div class=\"item-detail-stat-row\">Grants Photon Art: " + EscapeRml(photon_art_name) +
                      "</div>";
    }

    if (!entry->mod_slot_labels.empty())
        markup += "<div class=\"item-detail-stat-row\">Mod Slots: " + std::to_string(entry->mod_slot_labels.size()) +
                  "</div>";

    markup += "</div></div>";

    if (!entry->requirement_text.empty())
        markup += "<div class=\"item-detail-requirement" + std::string(entry->requirement_met ? "" : " unmet") +
                  "\">" + EscapeRml(entry->requirement_text) + "</div>";

    panel->SetInnerRML(markup);
    panel->SetProperty("display", "flex");
}

void HudLayer::RenderMagPanel()
{
    if (!m_document)
        return;

    Rml::Element* panel = m_document->GetElementById("character-screen-mag-panel");
    if (!panel)
        return;

    if (!m_character_screen_cache || !m_character_screen_cache->mag)
    {
        panel->SetProperty("display", "none");
        m_mag_panel_elements.reset();
        m_mag_stat_animations = {};
        return;
    }

    const CharacterScreenMessage::MagSummary& mag = *m_character_screen_cache->mag;
    const std::array<CharacterScreenMessage::MagStatBar, 4> bars = {mag.pow, mag.def, mag.dex, mag.mind};

    if (!m_mag_panel_elements)
    {
        static constexpr std::array<const char*, 4> kLabels = {"POW", "DEF", "DEX", "MIND"};

        std::string markup = "<div class=\"mag-panel-title\"></div>";
        for (const char* label : kLabels)
        {
            markup += std::string("<div class=\"mag-stat-row\"><span class=\"mag-stat-label\">") + label +
                      "</span><span class=\"mag-stat-level\"></span><div class=\"mag-stat-bar-track\">"
                      "<div class=\"mag-stat-bar-fill\"></div></div></div>";
        }
        markup += "<div class=\"mag-info-row\"></div>";
        panel->SetInnerRML(markup);

        MagPanelElements elements;
        elements.title = panel->QuerySelector(".mag-panel-title");
        Rml::ElementList rows;
        panel->QuerySelectorAll(rows, ".mag-stat-row");
        Rml::ElementList levels;
        panel->QuerySelectorAll(levels, ".mag-stat-level");
        Rml::ElementList fills;
        panel->QuerySelectorAll(fills, ".mag-stat-bar-fill");
        for (std::size_t i = 0; i < 4 && i < rows.size() && i < levels.size() && i < fills.size(); ++i)
        {
            elements.rows[i] = rows[i];
            elements.level_labels[i] = levels[i];
            elements.fills[i] = fills[i];
        }
        elements.info_row = panel->QuerySelector(".mag-info-row");
        m_mag_panel_elements = elements;

        // First time this mag's data is shown: snap straight to its actual
        // values instead of animating from a stale zero baseline.
        for (std::size_t i = 0; i < 4; ++i)
        {
            const int subunits = bars[i].level * bars[i].progress_to_next + bars[i].progress;
            m_mag_stat_animations[i] = MagStatAnimation{static_cast<float>(subunits), subunits, bars[i].level, 0.0f};
        }
    }
    else
    {
        for (std::size_t i = 0; i < 4; ++i)
            m_mag_stat_animations[i].target_subunits = bars[i].level * bars[i].progress_to_next + bars[i].progress;
    }

    ApplyMagPanelDisplay();
    RefreshMagPanelVisibility();
}

void HudLayer::ApplyMagPanelDisplay()
{
    if (!m_mag_panel_elements || !m_character_screen_cache || !m_character_screen_cache->mag)
        return;

    const CharacterScreenMessage::MagSummary& mag = *m_character_screen_cache->mag;
    const std::array<CharacterScreenMessage::MagStatBar, 4> bars = {mag.pow, mag.def, mag.dex, mag.mind};

    int displayed_total_level = 0;
    for (const MagStatAnimation& anim : m_mag_stat_animations)
        displayed_total_level += anim.displayed_level;

    if (m_mag_panel_elements->title)
        m_mag_panel_elements->title->SetInnerRML("Mag - Lv " + std::to_string(displayed_total_level));

    for (std::size_t i = 0; i < 4; ++i)
    {
        const MagStatAnimation& anim = m_mag_stat_animations[i];
        const float progress_to_next = static_cast<float>(std::max(bars[i].progress_to_next, 1));
        // Wrapped in float space (not truncated to int first) so the fill
        // moves continuously between whole sub-units instead of stepping in
        // 100/progress_to_next %-sized jumps as displayed_subunits ticks up.
        const float wrapped =
            anim.displayed_subunits - std::floor(anim.displayed_subunits / progress_to_next) * progress_to_next;
        const int percent = std::clamp(static_cast<int>(wrapped / progress_to_next * 100.0f + 0.5f), 0, 100);

        if (m_mag_panel_elements->level_labels[i])
            m_mag_panel_elements->level_labels[i]->SetInnerRML(std::to_string(anim.displayed_level));
        if (m_mag_panel_elements->fills[i])
            m_mag_panel_elements->fills[i]->SetProperty("width", std::to_string(percent) + "%");
        if (m_mag_panel_elements->rows[i])
            m_mag_panel_elements->rows[i]->SetClass("mag-level-up", anim.level_up_flash > 0.0f);
    }

    if (m_mag_panel_elements->info_row)
        m_mag_panel_elements->info_row->SetInnerRML("IQ: " + std::to_string(mag.iq) + "   Sync: " +
                                                     std::to_string(static_cast<int>(mag.sync)) + "%");
}

void HudLayer::UpdateMagPanelAnimations(float delta_time)
{
    if (!AnyMagStatAnimating())
        return;
    if (!m_mag_panel_elements || !m_character_screen_cache || !m_character_screen_cache->mag)
        return;

    constexpr float kMagBarFillUnitsPerSecond = 40.0f;
    constexpr float kMagLevelUpFlashSeconds = 0.5f;

    const CharacterScreenMessage::MagSummary& mag = *m_character_screen_cache->mag;
    const std::array<CharacterScreenMessage::MagStatBar, 4> bars = {mag.pow, mag.def, mag.dex, mag.mind};

    for (std::size_t i = 0; i < 4; ++i)
    {
        MagStatAnimation& anim = m_mag_stat_animations[i];
        const float target = static_cast<float>(anim.target_subunits);
        if (anim.displayed_subunits < target)
            anim.displayed_subunits =
                std::min(target, anim.displayed_subunits + kMagBarFillUnitsPerSecond * delta_time);
        else if (anim.displayed_subunits > target)
            anim.displayed_subunits =
                std::max(target, anim.displayed_subunits - kMagBarFillUnitsPerSecond * delta_time);

        const int progress_to_next = std::max(bars[i].progress_to_next, 1);
        const int new_level = static_cast<int>(anim.displayed_subunits) / progress_to_next;
        if (new_level > anim.displayed_level)
            anim.level_up_flash = kMagLevelUpFlashSeconds;
        anim.displayed_level = new_level;

        if (anim.level_up_flash > 0.0f)
            anim.level_up_flash = std::max(0.0f, anim.level_up_flash - delta_time);
    }

    ApplyMagPanelDisplay();
    RefreshMagPanelVisibility();
}

bool HudLayer::AnyMagStatAnimating() const
{
    for (const MagStatAnimation& anim : m_mag_stat_animations)
        if (anim.displayed_subunits != static_cast<float>(anim.target_subunits) || anim.level_up_flash > 0.0f)
            return true;
    return false;
}

void HudLayer::RefreshMagPanelVisibility()
{
    if (!m_document)
        return;

    Rml::Element* panel = m_document->GetElementById("character-screen-mag-panel");
    if (!panel)
        return;

    const bool mag_slot_selected =
        m_hovered_equipment_index == static_cast<int>(EquipmentSlot::Mag) ||
        (!m_hovered_equipment_index.has_value() && m_focused_panel == CharacterScreenPanel::Equipment &&
         m_focused_row == static_cast<int>(EquipmentSlot::Mag));

    const bool visible = m_character_screen_cache && m_character_screen_cache->mag &&
                         (mag_slot_selected || m_awaiting_mag_food_selection || AnyMagStatAnimating());
    panel->SetProperty("display", visible ? "flex" : "none");
}

void HudLayer::RenderRowFocus(const char* container_id, const char* row_class, CharacterScreenPanel panel)
{
    Rml::Element* container = m_document->GetElementById(container_id);
    if (!container)
        return;

    container->SetClass("focused", panel == m_focused_panel);

    Rml::ElementList rows;
    container->QuerySelectorAll(rows, row_class);
    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i]->SetClass("focused", panel == m_focused_panel && static_cast<int>(i) == m_focused_row);
}

Rml::Element* HudLayer::CharacterScreenRowElement(CharacterScreenPanel panel, int index)
{
    const char* container_id =
        panel == CharacterScreenPanel::Equipment ? "character-screen-equipment" : "character-screen-inventory";
    const char* row_class = panel == CharacterScreenPanel::Equipment ? ".equip-row" : ".inventory-row";

    Rml::Element* container = m_document->GetElementById(container_id);
    if (!container)
        return nullptr;

    Rml::ElementList rows;
    container->QuerySelectorAll(rows, row_class);
    if (index < 0 || index >= static_cast<int>(rows.size()))
        return nullptr;
    return rows[static_cast<std::size_t>(index)];
}

void HudLayer::OnEvent(Event& event)
{
    if (!m_document || (!m_character_screen_cache && !m_action_palette_cache && !m_mission_select_cache &&
                        !m_shop_cache && !m_storage_cache && !m_pause_cache && !m_confirm_cache))
        return;

    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>(
        [this](KeyPressedEvent& key_event)
        {
            const int key = key_event.GetKeyCode();

            if (m_awaiting_hotbar_assign_source != HotbarAssignSource::None)
            {
                if (key == SDLK_ESCAPE)
                {
                    CancelAwaitingHotbarSlot();
                    return true;
                }
                if (const std::optional<int> slot = KeyCodeToHotbarSlot(key))
                {
                    if (m_awaiting_hotbar_assign_source == HotbarAssignSource::CharacterScreenItem)
                    {
                        const int inventory_index = m_awaiting_hotbar_inventory_index;
                        CancelAwaitingHotbarSlot();
                        Publish(HotbarSlotAssignedMessage{inventory_index, *slot});
                    }
                    else
                    {
                        const HotbarSlotType type = m_awaiting_hotbar_ability_type;
                        const std::uint32_t id = m_awaiting_hotbar_ability_id;
                        CancelAwaitingHotbarSlot();
                        Publish(ActionPaletteSlotAssignedMessage{type, id, *slot});
                    }
                    return true;
                }
                return true; // swallow all other keys while awaiting
            }

            if (m_awaiting_mag_food_selection)
            {
                switch (key)
                {
                case SDLK_ESCAPE:
                    CancelAwaitingMagFood();
                    RenderFocusHighlights();
                    return true;
                case SDLK_KP_8:
                    MoveRowFocus(-1);
                    return true;
                case SDLK_KP_2:
                    MoveRowFocus(1);
                    return true;
                case SDLK_SPACE:
                case SDLK_KP_5:
                    ActivateFocusedRow();
                    return true;
                default:
                    return true; // swallow panel-switching and everything else while picking food
                }
            }

            if (m_pause_placeholder_open)
            {
                if (key == SDLK_ESCAPE || key == SDLK_SPACE || key == SDLK_KP_5)
                {
                    HidePausePlaceholder();
                    return true;
                }
                return true; // swallow everything else while the placeholder is up
            }

            if (m_confirm_cache)
            {
                switch (key)
                {
                case SDLK_KP_8:
                    MoveConfirmRowFocus(-1);
                    return true;
                case SDLK_KP_2:
                    MoveConfirmRowFocus(1);
                    return true;
                case SDLK_SPACE:
                case SDLK_KP_5:
                    ActivateFocusedConfirmRow();
                    return true;
                default:
                    return false;
                }
            }

            // Only reached while the pause menu itself has top focus (no
            // placeholder, no confirm dialog on top of it) -- Escape falls
            // through to PauseState's own HandleEvent, closing the whole
            // menu (an implicit Resume), same as every other screen here.
            if (m_pause_cache)
            {
                switch (key)
                {
                case SDLK_KP_8:
                    MovePauseRowFocus(-1);
                    return true;
                case SDLK_KP_2:
                    MovePauseRowFocus(1);
                    return true;
                case SDLK_SPACE:
                case SDLK_KP_5:
                    ActivateFocusedPauseRow();
                    return true;
                default:
                    return false;
                }
            }

            if (m_action_palette_cache)
            {
                switch (key)
                {
                case SDLK_KP_4:
                    MoveTechPanelFocus(-1);
                    return true;
                case SDLK_KP_6:
                    MoveTechPanelFocus(1);
                    return true;
                case SDLK_KP_8:
                    MoveTechRowFocus(-1);
                    return true;
                case SDLK_KP_2:
                    MoveTechRowFocus(1);
                    return true;
                case SDLK_SPACE:
                case SDLK_KP_5:
                    ActivateFocusedTechRow();
                    return true;
                default:
                    return false;
                }
            }

            if (m_mission_select_cache)
            {
                switch (key)
                {
                case SDLK_KP_8:
                    MoveMissionSelectRowFocus(-1);
                    return true;
                case SDLK_KP_2:
                    MoveMissionSelectRowFocus(1);
                    return true;
                case SDLK_SPACE:
                case SDLK_KP_5:
                    ActivateFocusedMissionSelectRow();
                    return true;
                default:
                    return false;
                }
            }

            if (m_shop_cache)
            {
                switch (key)
                {
                case SDLK_KP_4:
                    MoveShopPanelFocus(-1);
                    return true;
                case SDLK_KP_6:
                    MoveShopPanelFocus(1);
                    return true;
                case SDLK_KP_8:
                    MoveShopRowFocus(-1);
                    return true;
                case SDLK_KP_2:
                    MoveShopRowFocus(1);
                    return true;
                case SDLK_SPACE:
                case SDLK_KP_5:
                    ActivateFocusedShopRow();
                    return true;
                default:
                    return false;
                }
            }

            if (m_storage_cache)
            {
                switch (key)
                {
                case SDLK_KP_4:
                    MoveStoragePanelFocus(-1);
                    return true;
                case SDLK_KP_6:
                    MoveStoragePanelFocus(1);
                    return true;
                case SDLK_KP_8:
                    MoveStorageRowFocus(-1);
                    return true;
                case SDLK_KP_2:
                    MoveStorageRowFocus(1);
                    return true;
                case SDLK_SPACE:
                case SDLK_KP_5:
                    ActivateFocusedStorageRow();
                    return true;
                default:
                    return false;
                }
            }

            if (m_menu_open)
            {
                switch (key)
                {
                case SDLK_KP_8:
                    MoveMenuHighlight(-1);
                    return true;
                case SDLK_KP_2:
                    MoveMenuHighlight(1);
                    return true;
                case SDLK_SPACE:
                case SDLK_KP_5:
                    ChooseHighlightedMenuOption();
                    return true;
                case SDLK_ESCAPE:
                    CloseContextMenu();
                    RenderFocusHighlights();
                    return true;
                case SDLK_KP_4:
                case SDLK_KP_6:
                    return true; // swallow -- no panel-switch while a menu is open
                default:
                    return false;
                }
            }

            switch (key)
            {
            case SDLK_KP_4:
                MovePanelFocus(-1);
                return true;
            case SDLK_KP_6:
                MovePanelFocus(1);
                return true;
            case SDLK_KP_8:
                MoveRowFocus(-1);
                return true;
            case SDLK_KP_2:
                MoveRowFocus(1);
                return true;
            case SDLK_SPACE:
            case SDLK_KP_5:
                ActivateFocusedRow();
                return true;
            default:
                return false;
            }
        });
}

void HudLayer::OnFloatingTextState(const FloatingTextStateMessage& message)
{
    if (!m_document)
        return;

    Rml::Element* layer = m_document->GetElementById("floating-text-layer");
    if (!layer)
        return;

    std::string markup;
    for (const FloatingTextStateMessage::Entry& entry : message.entries)
    {
        // A real-size flex anchor box, positioned so it's centered on
        // (screen_x, screen_y) by construction (left/top offset by half the
        // box's own width/height), with its .floating-text child centered
        // within that box by align-items/justify-content -- see the
        // .floating-text-anchor doc comment in hud.rcss for why the box
        // can't be zero-size.
        const float box_width = kFloatingTextAnchorBaseWidth * entry.scale;
        const float box_height = kFloatingTextAnchorBaseHeight * entry.scale;
        markup += "<div class=\"floating-text-anchor\" style=\"left:" + std::to_string(entry.screen_x - box_width / 2.0f) +
                  "px; top:" + std::to_string(entry.screen_y - box_height / 2.0f) + "px; width:" +
                  std::to_string(box_width) + "px; height:" + std::to_string(box_height) +
                  "px;\"><span class=\"floating-text\" style=\"font-size:" +
                  std::to_string(kFloatingTextBaseFontSizeEm * entry.scale) + "em; color:" +
                  ColorToRgbaCss(entry.color) + ";\">" + EscapeRml(entry.text) + "</span></div>";
    }
    layer->SetInnerRML(markup);
}

} // namespace psr
