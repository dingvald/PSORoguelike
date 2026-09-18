#include "Layers/CharacterSlotSelectLayer.h"

#include "ApplicationFilepaths.h"
#include "Engine/Events/KeyEvent.h"
#include "Layers/GameplayLayer.h"
#include "Layers/MainMenuLayer.h"
#include "Persistence/CharacterSaveFile.h"
#include "UI/RmlClickListener.h"
#include "UI/RmlText.h"

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace psr {

namespace {
    std::filesystem::path FontPath = ApplicationFilepaths::FontsPath / "PixelCode-Regular.ttf";
    std::filesystem::path FontPathBold = ApplicationFilepaths::FontsPath / "PixelCode-Bold.ttf";
    std::filesystem::path DocumentPath = ApplicationFilepaths::RmlDocumentsPath / "character_slot_select.rml";

    std::string Capitalize(std::string_view text)
    {
        std::string result{text};
        if (!result.empty())
            result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
        return result;
    }

    std::string ClassLabel(ClassId class_id)
    {
        for (const auto& [text, value] : EnumNames<ClassId>::kValues)
            if (value == class_id)
                return Capitalize(text);
        return "";
    }

    std::string SectionIdLabel(SectionId section_id)
    {
        for (const auto& [text, value] : EnumNames<SectionId>::kValues)
            if (value == section_id)
                return Capitalize(text);
        return "";
    }

} // namespace

CharacterSlotSelectLayer::CharacterSlotSelectLayer() : Layer("CharacterSlotSelectLayer") {}
CharacterSlotSelectLayer::~CharacterSlotSelectLayer() = default;

void CharacterSlotSelectLayer::OnAttach()
{
    if (!Rml::LoadFontFace(FontPath.string().c_str()))
        SDL_Log("Warning: CharacterSlotSelectLayer failed to load font '%s'", FontPath.string().c_str());
    if (!Rml::LoadFontFace(FontPathBold.string().c_str()))
        SDL_Log("Warning: CharacterSlotSelectLayer failed to load font '%s'", FontPathBold.string().c_str());

    GuiContext::LockedAccess gui_context = GetLockedGuiContext();
    m_document = gui_context->LoadDocument(DocumentPath.string().c_str());
    if (!m_document)
    {
        SDL_Log("Warning: CharacterSlotSelectLayer has no document to show");
        return;
    }

    Rml::Element* list = m_document->GetElementById("slot-list");
    if (!list)
        return;

    std::string markup;
    m_slot_occupied.clear();
    for (int slot = 0; slot < kMaxCharacterSaveSlots; ++slot)
    {
        std::optional<CharacterSaveSummary> summary = ReadCharacterSaveSummary(slot);
        m_slot_occupied.push_back(summary.has_value());

        if (summary)
        {
            const std::string desc = ClassLabel(summary->class_id) + " | " + SectionIdLabel(summary->section_id) +
                                     " | Lv " + std::to_string(summary->level) + " | " +
                                     std::to_string(summary->meseta) + " Meseta";
            markup += "<li class=\"menu-row\"><span class=\"row-title\">" + EscapeRml(summary->name) +
                      "</span><span class=\"row-desc\">" + EscapeRml(desc) + "</span></li>";
        }
        else
        {
            markup += "<li class=\"menu-row disabled\"><span class=\"row-title\">Empty</span></li>";
        }
    }
    list->SetInnerRML(markup);

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".menu-row");
    for (std::size_t i = 0; i < rows.size() && i < m_slot_occupied.size(); ++i)
    {
        if (!m_slot_occupied[i])
            continue;
        const int index = static_cast<int>(i);
        auto listener = std::make_unique<RmlClickListener>(
            [this, index]
            {
                m_selected_index = index;
                RefreshSelectionHighlight();
                ConfirmSelection();
            });
        listener->Attach(*rows[i]);
        m_listeners.push_back(std::move(listener));
    }

    m_selected_index = 0;
    for (std::size_t i = 0; i < m_slot_occupied.size(); ++i)
        if (m_slot_occupied[i])
        {
            m_selected_index = static_cast<int>(i);
            break;
        }
    RefreshSelectionHighlight();

    m_document->Show();
}

void CharacterSlotSelectLayer::OnDetach()
{
    m_listeners.clear();
    if (m_document)
    {
        m_document->Close();
        m_document = nullptr;
    }
}

void CharacterSlotSelectLayer::OnEvent(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) { return OnKeyPressed(e); });
}

bool CharacterSlotSelectLayer::OnKeyPressed(KeyPressedEvent& event)
{
    if (event.IsRepeat())
        return false;

    const int key_code = event.GetKeyCode();
    if (key_code == SDLK_UP || key_code == SDLK_KP_8)
    {
        MoveSelection(-1);
        return true;
    }
    if (key_code == SDLK_DOWN || key_code == SDLK_KP_2)
    {
        MoveSelection(1);
        return true;
    }
    if (key_code == SDLK_RETURN || key_code == SDLK_KP_ENTER || key_code == SDLK_SPACE)
    {
        ConfirmSelection();
        return true;
    }
    if (key_code == SDLK_ESCAPE)
    {
        TransitionTo<MainMenuLayer>();
        return true;
    }
    return false;
}

void CharacterSlotSelectLayer::RefreshSelectionHighlight()
{
    if (!m_document)
        return;
    Rml::Element* list = m_document->GetElementById("slot-list");
    if (!list)
        return;

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".menu-row");
    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i]->SetClass("selected", static_cast<int>(i) == m_selected_index);
}

void CharacterSlotSelectLayer::MoveSelection(int delta)
{
    const int count = static_cast<int>(m_slot_occupied.size());
    if (count <= 0)
        return;
    int next = m_selected_index;
    for (int step = 0; step < count; ++step)
    {
        next = (next + delta + count) % count;
        if (m_slot_occupied[static_cast<std::size_t>(next)])
            break;
    }
    m_selected_index = next;
    RefreshSelectionHighlight();
}

void CharacterSlotSelectLayer::SelectIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(m_slot_occupied.size()) || !m_slot_occupied[static_cast<std::size_t>(index)])
        return;
    m_selected_index = index;
    RefreshSelectionHighlight();
    ConfirmSelection();
}

void CharacterSlotSelectLayer::ConfirmSelection()
{
    if (m_selected_index < 0 || m_selected_index >= static_cast<int>(m_slot_occupied.size()) ||
        !m_slot_occupied[static_cast<std::size_t>(m_selected_index)])
        return;
    TransitionTo<GameplayLayer>(m_selected_index);
}

} // namespace psr
