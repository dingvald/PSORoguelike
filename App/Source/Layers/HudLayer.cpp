#include "Layers/HudLayer.h"

#include "Messages/CharacterScreenClosedMessage.h"
#include "Messages/CharacterScreenMessage.h"
#include "Messages/CombatLogEntryMessage.h"
#include "Messages/EquipmentSlotActivatedMessage.h"
#include "Messages/FloatingTextStateMessage.h"
#include "Messages/GameRestartedMessage.h"
#include "Messages/HotbarSlotActivatedMessage.h"
#include "Messages/HotbarStateMessage.h"
#include "Messages/HudReadyMessage.h"
#include "Messages/InventoryItemActivatedMessage.h"
#include "Messages/InventoryItemHoverChangedMessage.h"
#include "Messages/LootDropMessage.h"
#include "Messages/MesetaChangedMessage.h"
#include "Messages/PlayerDefeatedMessage.h"
#include "Messages/PlayerStatusMessage.h"
#include "Messages/StatusEffectsMessage.h"

#include "ApplicationFilepaths.h"
#include "Engine/Math/Color.h"
#include "Items/Equip.h"
#include "UI/RmlClickListener.h"
#include "UI/RmlHoverListener.h"
#include "UI/RmlText.h"

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace psr {

namespace {
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
    std::string ColorToRgbaCss(Color color)
    {
        return "rgba(" + std::to_string(color.r) + "," + std::to_string(color.g) + "," + std::to_string(color.b) +
               "," + std::to_string(static_cast<float>(color.a) / 255.0f) + ")";
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
    Subscribe<MesetaChangedMessage>(&HudLayer::OnMesetaChanged, this);
    Subscribe<CharacterScreenMessage>(&HudLayer::OnCharacterScreenState, this);
    Subscribe<CharacterScreenClosedMessage>(&HudLayer::OnCharacterScreenClosed, this);
    Subscribe<FloatingTextStateMessage>(&HudLayer::OnFloatingTextState, this);

    // Tells GameplayLayer to re-publish current state now that this layer is
    // actually subscribed -- see HudReadyMessage.h for why a one-time publish
    // from GameplayLayer::OnAttach can't reach this layer directly.
    Publish(HudReadyMessage{});
}

void HudLayer::OnDetach()
{
    m_hotbar_listeners.clear();
    m_character_screen_listeners.clear();
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

void HudLayer::OnMesetaChanged(const MesetaChangedMessage& message)
{
    if (!m_document)
        return;

    if (Rml::Element* text = m_document->GetElementById("meseta-text"))
        text->SetInnerRML(EscapeRml(std::to_string(message.current_meseta)));
}

void HudLayer::OnCharacterScreenState(const CharacterScreenMessage& message)
{
    if (!m_document)
        return;

    if (Rml::Element* overlay = m_document->GetElementById("character-screen"))
        overlay->SetProperty("display", "flex");

    m_character_screen_listeners.clear();
    m_character_screen_hover_listeners.clear();

    static constexpr std::array<const char*, 5> kSlotLabels = {"Weapon", "Head", "Torso", "Hands", "Legs"};

    if (Rml::Element* equipment_list = m_document->GetElementById("character-screen-equipment"))
    {
        std::string markup;
        for (std::size_t i = 0; i < message.equipment.size(); ++i)
        {
            const std::string label =
                message.equipment[i] ? EscapeRml(message.equipment[i]->display_name) : std::string("(empty)");
            const bool focused = message.focus.equipment_slot == static_cast<EquipmentSlot>(i);
            markup += std::string("<div class=\"equip-row") + (focused ? " focused" : "") + "\">" + kSlotLabels[i] +
                     ": " + label + "</div>";
        }
        equipment_list->SetInnerRML(markup);

        Rml::ElementList rows;
        equipment_list->QuerySelectorAll(rows, ".equip-row");
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const EquipmentSlot slot = static_cast<EquipmentSlot>(i);
            auto listener =
                std::make_unique<RmlClickListener>([this, slot]() { Publish(EquipmentSlotActivatedMessage{slot}); });
            listener->Attach(*rows[i]);
            m_character_screen_listeners.push_back(std::move(listener));
        }
    }

    if (Rml::Element* inventory_list = m_document->GetElementById("character-screen-inventory"))
    {
        std::string markup;
        for (std::size_t i = 0; i < message.inventory.size(); ++i)
        {
            const bool focused = message.focus.inventory_index == static_cast<int>(i);
            markup += std::string("<div class=\"inventory-row") + (focused ? " focused" : "") + "\">" +
                     EscapeRml(message.inventory[i].display_name) + "</div>";
        }
        inventory_list->SetInnerRML(markup);

        Rml::ElementList rows;
        inventory_list->QuerySelectorAll(rows, ".inventory-row");
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const int index = static_cast<int>(i);
            auto listener =
                std::make_unique<RmlClickListener>([this, index]() { Publish(InventoryItemActivatedMessage{index}); });
            listener->Attach(*rows[i]);
            m_character_screen_listeners.push_back(std::move(listener));

            auto hover_listener = std::make_unique<RmlHoverListener>(
                [this, index]() { Publish(InventoryItemHoverChangedMessage{index}); },
                [this]() { Publish(InventoryItemHoverChangedMessage{std::nullopt}); });
            hover_listener->Attach(*rows[i]);
            m_character_screen_hover_listeners.push_back(std::move(hover_listener));
        }
    }

    if (Rml::Element* stats_panel = m_document->GetElementById("character-screen-stats"))
    {
        std::string markup = "<div class=\"stat-row\">Level " + std::to_string(message.level) + "</div>";
        markup += "<div class=\"stat-row\">EXP " + std::to_string(message.current_exp) + " / " +
                 std::to_string(message.exp_to_next_level) + "</div>";

        for (const CharacterScreenMessage::StatEntry& stat : message.stats)
        {
            std::string row_class = "stat-row";
            std::string text = EscapeRml(stat.label) + " " + std::to_string(stat.current_value);
            if (stat.preview_value)
            {
                row_class += *stat.preview_value > stat.current_value ? " stat-positive" : " stat-negative";
                text += " -&gt; " + std::to_string(*stat.preview_value);
            }
            markup += "<div class=\"" + row_class + "\">" + text + "</div>";
        }

        stats_panel->SetInnerRML(markup);
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
                  "px; top:" + std::to_string(entry.screen_y) + "px; color:" + ColorToRgbaCss(entry.color) +
                  ";\">" + EscapeRml(entry.text) + "</span>";
    }
    layer->SetInnerRML(markup);
}

} // namespace psr
