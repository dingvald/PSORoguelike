#include "Layers/HudLayer.h"

#include "Messages/CharacterScreenClosedMessage.h"
#include "Messages/CharacterScreenMessage.h"
#include "Messages/CombatLogEntryMessage.h"
#include "Messages/EquipmentSlotActivatedMessage.h"
#include "Messages/FloatingTextStateMessage.h"
#include "Messages/GameRestartedMessage.h"
#include "Messages/HotbarSlotActivatedMessage.h"
#include "Messages/HotbarSlotAssignedMessage.h"
#include "Messages/HotbarStateMessage.h"
#include "Messages/HudReadyMessage.h"
#include "Messages/InventoryItemActivatedMessage.h"
#include "Messages/LootDropMessage.h"
#include "Messages/PlayerDefeatedMessage.h"
#include "Messages/PlayerStatusMessage.h"
#include "Messages/StatusEffectsMessage.h"
#include "Messages/TargetStateMessage.h"
#include "Messages/TechniquesScreenClosedMessage.h"
#include "Messages/TechniquesScreenSlotAssignedMessage.h"

#include "ApplicationFilepaths.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Math/Color.h"
#include "Items/Equip.h"
#include "UI/RmlClickListener.h"
#include "UI/RmlText.h"

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace psr {

namespace {

    // Must match #character-screen-context-menu's width/max-height in
    // hud.rcss -- RenderContextMenu positions using these fixed constants
    // rather than the element's own live box, to avoid a same-call
    // pre-layout read (same reasoning as PieceEditorLayer's painter dropdown).
    constexpr float kContextMenuWidth = 180.0f;
    constexpr float kContextMenuMaxHeight = 200.0f;

    // Must match #character-screen-hint's initial text in hud.rml -- swapped
    // back in by CancelAwaitingHotbarSlot, same "must match markup"
    // reasoning as kContextMenuWidth/kContextMenuMaxHeight above.
    constexpr const char* kDefaultCharacterScreenHint = "Numpad to navigate, Space to select, C / Esc to close";
    constexpr const char* kAwaitingHotbarSlotHint = "Press 0-9 to assign to a hotbar slot (Esc to cancel)";

    // Must match #techniques-screen-hint's initial text in hud.rml -- same
    // "must match markup" reasoning as kDefaultCharacterScreenHint.
    constexpr const char* kDefaultTechniquesScreenHint = "Numpad to navigate, Space to assign to hotbar, T / Esc to close";

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

    Subscribe<PlayerStatusMessage>(&HudLayer::OnPlayerStatus, this);
    Subscribe<HotbarStateMessage>(&HudLayer::OnHotbarState, this);
    Subscribe<CombatLogEntryMessage>(&HudLayer::OnLogEntry, this);
    Subscribe<StatusEffectsMessage>(&HudLayer::OnStatusEffects, this);
    Subscribe<PlayerDefeatedMessage>(&HudLayer::OnPlayerDefeated, this);
    Subscribe<GameRestartedMessage>(&HudLayer::OnGameRestarted, this);
    Subscribe<LootDropMessage>(&HudLayer::OnLootDrop, this);
    Subscribe<CharacterScreenMessage>(&HudLayer::OnCharacterScreenState, this);
    Subscribe<CharacterScreenClosedMessage>(&HudLayer::OnCharacterScreenClosed, this);
    Subscribe<TechniquesScreenMessage>(&HudLayer::OnTechniquesScreenState, this);
    Subscribe<TechniquesScreenClosedMessage>(&HudLayer::OnTechniquesScreenClosed, this);
    Subscribe<FloatingTextStateMessage>(&HudLayer::OnFloatingTextState, this);
    Subscribe<TargetStateMessage>(&HudLayer::OnTargetState, this);

    // Tells GameplayLayer to re-publish current state now that this layer is
    // actually subscribed -- see HudReadyMessage.h for why a one-time publish
    // from GameplayLayer::OnAttach can't reach this layer directly.
    Publish(HudReadyMessage{});
}

void HudLayer::OnDetach()
{
    m_hotbar_listeners.clear();
    m_character_screen_listeners.clear();
    m_techniques_screen_listeners.clear();
    m_context_menu_listeners.clear();
    if (m_document)
    {
        m_document->Close();
        m_document = nullptr;
    }
}

void HudLayer::OnUpdate(float /*delta_time*/) { HandleQueuedMessages(); }

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
        markup += "<div class=\"log-line\">" + EscapeRml(line) + "</div>";
    log->SetInnerRML(markup);
    log->SetScrollTop(log->GetScrollHeight());
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
    if (Rml::Element* log = m_document->GetElementById("event-log"))
        log->SetInnerRML("");
}

void HudLayer::OnLootDrop(const LootDropMessage& message) { AppendLogLine("Found " + message.item_name); }

void HudLayer::OnCharacterScreenState(const CharacterScreenMessage& message)
{
    if (!m_document)
        return;

    const bool fresh_open = !m_character_screen_cache.has_value();
    m_character_screen_cache = message;
    CloseContextMenu();
    CancelAwaitingHotbarSlot();

    if (Rml::Element* overlay = m_document->GetElementById("character-screen"))
        overlay->SetProperty("display", "flex");

    m_character_screen_listeners.clear();

    RenderStatsPanel(message.stats);

    static constexpr std::array<const char*, 5> kSlotLabels = {"Weapon", "Head", "Torso", "Hands", "Legs"};

    if (Rml::Element* equipment_list = m_document->GetElementById("character-screen-equipment"))
    {
        std::string markup;
        for (std::size_t i = 0; i < message.equipment.size(); ++i)
        {
            const std::string label =
                message.equipment[i] ? EscapeRml(message.equipment[i]->display_name) : std::string("(empty)");
            std::string mod_slots_markup;
            if (message.equipment[i])
                for (const std::string& mod_slot_label : message.equipment[i]->mod_slot_labels)
                    mod_slots_markup +=
                        "<div class=\"mod-slot-row\">\xE2\x80\xA2 " + EscapeRml(mod_slot_label) + "</div>";
            markup +=
                std::string("<div class=\"equip-row\">") + kSlotLabels[i] + ": " + label + "</div>" + mod_slots_markup;
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
        }
    }

    if (Rml::Element* inventory_list = m_document->GetElementById("character-screen-inventory"))
    {
        std::string markup;
        for (const CharacterScreenMessage::ItemEntry& entry : message.inventory)
            markup += "<div class=\"inventory-row\">" + EscapeRml(entry.display_name) + "</div>";
        inventory_list->SetInnerRML(markup);

        Rml::ElementList rows;
        inventory_list->QuerySelectorAll(rows, ".inventory-row");
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const int index = static_cast<int>(i);
            auto listener = std::make_unique<RmlClickListener>(
                [this, index]() { OpenContextMenu(CharacterScreenPanel::Inventory, index); });
            listener->Attach(*rows[i]);
            m_character_screen_listeners.push_back(std::move(listener));
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
}

void HudLayer::OnCharacterScreenClosed(const CharacterScreenClosedMessage& /*message*/)
{
    if (!m_document)
        return;

    if (Rml::Element* overlay = m_document->GetElementById("character-screen"))
        overlay->SetProperty("display", "none");

    m_character_screen_listeners.clear();
    CloseContextMenu();
    CancelAwaitingHotbarSlot();
    m_character_screen_cache.reset();
    m_focused_panel = CharacterScreenPanel::Stats;
    m_focused_row = 0;
}

void HudLayer::OnTechniquesScreenState(const TechniquesScreenMessage& message)
{
    if (!m_document)
        return;

    const bool fresh_open = !m_techniques_screen_cache.has_value();
    m_techniques_screen_cache = message;
    CancelAwaitingHotbarSlot();

    if (Rml::Element* overlay = m_document->GetElementById("techniques-screen"))
        overlay->SetProperty("display", "flex");

    m_techniques_screen_listeners.clear();

    if (Rml::Element* list = m_document->GetElementById("techniques-screen-techniques"))
    {
        std::string markup;
        if (message.techniques.empty())
        {
            markup = "<div class=\"list-empty\">No Techniques learned yet.</div>";
        }
        else
        {
            for (const TechniquesScreenMessage::TechniqueEntry& entry : message.techniques)
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
                    m_tech_focused_panel = TechniquesScreenPanel::Techniques;
                    m_tech_focused_row = index;
                    ActivateFocusedTechRow();
                });
            listener->Attach(*rows[i]);
            m_techniques_screen_listeners.push_back(std::move(listener));
        }
    }

    if (Rml::Element* list = m_document->GetElementById("techniques-screen-photon-arts"))
    {
        std::string markup;
        if (message.photon_arts.empty())
        {
            markup = "<div class=\"list-empty\">No Photon Arts granted by the equipped weapon.</div>";
        }
        else
        {
            for (const TechniquesScreenMessage::PhotonArtEntry& entry : message.photon_arts)
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
                    m_tech_focused_panel = TechniquesScreenPanel::PhotonArts;
                    m_tech_focused_row = index;
                    ActivateFocusedTechRow();
                });
            listener->Attach(*rows[i]);
            m_techniques_screen_listeners.push_back(std::move(listener));
        }
    }

    if (fresh_open)
    {
        m_tech_focused_panel = TechniquesScreenPanel::Techniques;
        m_tech_focused_row = 0;
    }
    else
    {
        m_tech_focused_row = std::clamp(m_tech_focused_row, 0, std::max(0, TechniquesScreenRowCount(m_tech_focused_panel) - 1));
    }
    RenderTechniquesFocusHighlights();
}

void HudLayer::OnTechniquesScreenClosed(const TechniquesScreenClosedMessage& /*message*/)
{
    if (!m_document)
        return;

    if (Rml::Element* overlay = m_document->GetElementById("techniques-screen"))
        overlay->SetProperty("display", "none");

    m_techniques_screen_listeners.clear();
    CancelAwaitingHotbarSlot();
    m_techniques_screen_cache.reset();
    m_tech_focused_panel = TechniquesScreenPanel::Techniques;
    m_tech_focused_row = 0;
}

int HudLayer::TechniquesScreenRowCount(TechniquesScreenPanel panel) const
{
    if (!m_techniques_screen_cache)
        return 0;

    return panel == TechniquesScreenPanel::Techniques
               ? static_cast<int>(m_techniques_screen_cache->techniques.size())
               : static_cast<int>(m_techniques_screen_cache->photon_arts.size());
}

void HudLayer::MoveTechPanelFocus(int direction)
{
    constexpr int kPanelCount = 2;
    const int next = std::clamp(static_cast<int>(m_tech_focused_panel) + direction, 0, kPanelCount - 1);
    m_tech_focused_panel = static_cast<TechniquesScreenPanel>(next);
    m_tech_focused_row = std::clamp(m_tech_focused_row, 0, std::max(0, TechniquesScreenRowCount(m_tech_focused_panel) - 1));
    RenderTechniquesFocusHighlights();
}

void HudLayer::MoveTechRowFocus(int direction)
{
    const int count = TechniquesScreenRowCount(m_tech_focused_panel);
    if (count <= 0)
        return;

    m_tech_focused_row = std::clamp(m_tech_focused_row + direction, 0, count - 1);
    RenderTechniquesFocusHighlights();
}

void HudLayer::ActivateFocusedTechRow()
{
    if (!m_techniques_screen_cache)
        return;

    if (m_tech_focused_panel == TechniquesScreenPanel::Techniques)
    {
        if (m_tech_focused_row < 0 || m_tech_focused_row >= static_cast<int>(m_techniques_screen_cache->techniques.size()))
            return;
        const TechniquesScreenMessage::TechniqueEntry& entry =
            m_techniques_screen_cache->techniques[static_cast<std::size_t>(m_tech_focused_row)];
        BeginAwaitingAbilityHotbarSlot(HotbarSlotType::Technique, entry.technique_id);
    }
    else
    {
        if (m_tech_focused_row < 0 || m_tech_focused_row >= static_cast<int>(m_techniques_screen_cache->photon_arts.size()))
            return;
        const TechniquesScreenMessage::PhotonArtEntry& entry =
            m_techniques_screen_cache->photon_arts[static_cast<std::size_t>(m_tech_focused_row)];
        BeginAwaitingAbilityHotbarSlot(HotbarSlotType::PhotonArt, entry.photon_art_id);
    }
}

void HudLayer::RenderTechniquesFocusHighlights()
{
    if (!m_document)
        return;

    RenderTechRowFocus("techniques-screen-techniques", ".technique-row", TechniquesScreenPanel::Techniques);
    RenderTechRowFocus("techniques-screen-photon-arts", ".photon-art-row", TechniquesScreenPanel::PhotonArts);
}

void HudLayer::RenderTechRowFocus(const char* container_id, const char* row_class, TechniquesScreenPanel panel)
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

void HudLayer::RenderStatsPanel(const CharacterScreenMessage::StatsSummary& stats)
{
    Rml::Element* panel = m_document->GetElementById("character-screen-stats");
    if (!panel)
        return;

    std::string markup;
    markup += "<div class=\"stat-row\">HP: " + std::to_string(stats.hp) + " / " + std::to_string(stats.max_hp) + "</div>";
    markup += "<div class=\"stat-row\">TP: " + std::to_string(stats.tp) + " / " + std::to_string(stats.max_tp) + "</div>";
    markup += "<div class=\"stat-row\">ATP: " + std::to_string(stats.atp) + "</div>";
    markup += "<div class=\"stat-row\">ATA: " + std::to_string(stats.ata) + "</div>";
    markup += "<div class=\"stat-row\">MST: " + std::to_string(stats.mst) + "</div>";
    markup += "<div class=\"stat-row\">DFP: " + std::to_string(stats.dfp) + "</div>";
    markup += "<div class=\"stat-row\">EVP: " + std::to_string(stats.evp) + "</div>";
    markup += "<div class=\"stat-row\">LCK: " + std::to_string(stats.lck) + "</div>";
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

    const ContextMenuOption::Action action = m_menu_options[static_cast<std::size_t>(m_menu_highlight)].action;
    const CharacterScreenPanel panel = m_menu_panel;
    const int index = m_menu_index;

    if (action == ContextMenuOption::Action::AssignToHotbar)
    {
        BeginAwaitingHotbarSlot(index);
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
    m_awaiting_hotbar_assign_source = HotbarAssignSource::TechniquesScreenAbility;
    m_awaiting_hotbar_ability_type = type;
    m_awaiting_hotbar_ability_id = id;
    SetTechniquesScreenHint(kAwaitingHotbarSlotHint, /*awaiting=*/true);
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
    if (m_techniques_screen_cache)
        SetTechniquesScreenHint(kDefaultTechniquesScreenHint, /*awaiting=*/false);
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

void HudLayer::SetTechniquesScreenHint(const char* text, bool awaiting)
{
    if (!m_document)
        return;

    Rml::Element* hint = m_document->GetElementById("techniques-screen-hint");
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
            options.push_back({ContextMenuOption::Action::Remove, "Remove"});
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
        markup += "<div class=\"menu-row\">" + EscapeRml(option.label) + "</div>";
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
    if (!m_document || (!m_character_screen_cache && !m_techniques_screen_cache))
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
                        Publish(TechniquesScreenSlotAssignedMessage{type, id, *slot});
                    }
                    return true;
                }
                return true; // swallow all other keys while awaiting
            }

            if (m_techniques_screen_cache)
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
        markup += "<span class=\"floating-text\" style=\"left:" + std::to_string(entry.screen_x) +
                  "px; top:" + std::to_string(entry.screen_y) + "px; color:" + ColorToRgbaCss(entry.color) + ";\">" +
                  EscapeRml(entry.text) + "</span>";
    }
    layer->SetInnerRML(markup);
}

} // namespace psr
