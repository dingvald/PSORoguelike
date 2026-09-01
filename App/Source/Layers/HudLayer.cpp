#include "Layers/HudLayer.h"

#include "Messages/CombatLogEntryMessage.h"
#include "Messages/GameRestartedMessage.h"
#include "Messages/HotbarSlotActivatedMessage.h"
#include "Messages/HotbarStateMessage.h"
#include "Messages/HudReadyMessage.h"
#include "Messages/PlayerDefeatedMessage.h"
#include "Messages/PlayerStatusMessage.h"
#include "Messages/StatusEffectsMessage.h"

#include "ApplicationFilepaths.h"
#include "UI/RmlClickListener.h"
#include "UI/RmlText.h"

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>

#include <algorithm>

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

    // Tells GameplayLayer to re-publish current state now that this layer is
    // actually subscribed -- see HudReadyMessage.h for why a one-time publish
    // from GameplayLayer::OnAttach can't reach this layer directly.
    Publish(HudReadyMessage{});
}

void HudLayer::OnDetach()
{
    m_hotbar_listeners.clear();
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

void HudLayer::OnLogEntry(const CombatLogEntryMessage& message)
{
    if (!m_document)
        return;

    m_log_lines.push_back(message.text);
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

} // namespace psr
