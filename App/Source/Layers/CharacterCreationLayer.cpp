#include "Layers/CharacterCreationLayer.h"

#include "ApplicationFilepaths.h"
#include "Engine/Events/KeyEvent.h"
#include "Layers/GameplayLayer.h"
#include "Layers/MainMenuLayer.h"
#include "UI/RmlClickListener.h"
#include "UI/RmlText.h"

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

#include <array>
#include <cctype>
#include <string>
#include <utility>

namespace psr {

namespace {
    std::filesystem::path FontPath = ApplicationFilepaths::FontsPath / "PixelCode-Regular.ttf";
    std::filesystem::path FontPathBold = ApplicationFilepaths::FontsPath / "PixelCode-Bold.ttf";
    std::filesystem::path DocumentPath = ApplicationFilepaths::RmlDocumentsPath / "character_creation.rml";

    // GDD's own one-line class blurbs (docs/GDD.md's "Class triad" section).
    std::string_view ClassDescription(ClassId class_id)
    {
        switch (class_id)
        {
        case ClassId::Hunter:
            return "Melee, HP-tanky front-liner";
        case ClassId::Ranger:
            return "Weapon-type-driven ranged damage";
        case ClassId::Force:
            return "Elemental Technique caster";
        }
        return "";
    }

    std::string Capitalize(std::string_view text)
    {
        std::string result{text};
        if (!result.empty())
            result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
        return result;
    }

} // namespace

CharacterCreationLayer::CharacterCreationLayer() : Layer("CharacterCreationLayer") {}
CharacterCreationLayer::~CharacterCreationLayer() = default;

void CharacterCreationLayer::OnAttach()
{
    if (!Rml::LoadFontFace(FontPath.string().c_str()))
        SDL_Log("Warning: CharacterCreationLayer failed to load font '%s'", FontPath.string().c_str());
    if (!Rml::LoadFontFace(FontPathBold.string().c_str()))
        SDL_Log("Warning: CharacterCreationLayer failed to load font '%s'", FontPathBold.string().c_str());

    GuiContext::LockedAccess gui_context = GetLockedGuiContext();
    m_document = gui_context->LoadDocument(DocumentPath.string().c_str());
    if (!m_document)
    {
        SDL_Log("Warning: CharacterCreationLayer has no document to show");
        return;
    }

    ShowClassStep();
    m_document->Show();
}

void CharacterCreationLayer::OnDetach()
{
    m_listeners.clear();
    if (m_document)
    {
        m_document->Close();
        m_document = nullptr;
    }
}

void CharacterCreationLayer::OnEvent(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) { return OnKeyPressed(e); });
}

bool CharacterCreationLayer::OnKeyPressed(KeyPressedEvent& event)
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
        if (m_step == Step::SectionId)
            ShowClassStep();
        else
            TransitionTo<MainMenuLayer>();
        return true;
    }
    return false;
}

void CharacterCreationLayer::ShowClassStep()
{
    if (!m_document)
        return;
    m_step = Step::Class;
    m_selected_index = 0;
    m_listeners.clear();

    if (Rml::Element* class_panel = m_document->GetElementById("class-panel"))
        class_panel->SetProperty("display", "flex");
    if (Rml::Element* section_panel = m_document->GetElementById("section-panel"))
        section_panel->SetProperty("display", "none");

    Rml::Element* list = m_document->GetElementById("class-list");
    if (!list)
        return;

    std::string markup;
    for (const auto& [text, value] : EnumNames<ClassId>::kValues)
        markup += "<li class=\"menu-row\"><span class=\"row-title\">" + EscapeRml(Capitalize(text)) +
                  "</span><span class=\"row-desc\">" + EscapeRml(std::string(ClassDescription(value))) +
                  "</span></li>";
    list->SetInnerRML(markup);

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".menu-row");
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
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

    RefreshSelectionHighlight();
}

void CharacterCreationLayer::ShowSectionStep()
{
    if (!m_document)
        return;
    m_step = Step::SectionId;
    m_selected_index = 0;
    m_listeners.clear();

    if (Rml::Element* class_panel = m_document->GetElementById("class-panel"))
        class_panel->SetProperty("display", "none");
    if (Rml::Element* section_panel = m_document->GetElementById("section-panel"))
        section_panel->SetProperty("display", "flex");

    Rml::Element* list = m_document->GetElementById("section-list");
    if (!list)
        return;

    std::string markup;
    for (const auto& [text, value] : EnumNames<SectionId>::kValues)
    {
        (void)value;
        markup += "<li class=\"menu-row\"><span class=\"row-title\">" + EscapeRml(Capitalize(text)) + "</span></li>";
    }
    list->SetInnerRML(markup);

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".menu-row");
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
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

    RefreshSelectionHighlight();
}

void CharacterCreationLayer::MoveSelection(int delta)
{
    const int count = m_step == Step::Class ? static_cast<int>(EnumNames<ClassId>::kValues.size())
                                            : static_cast<int>(EnumNames<SectionId>::kValues.size());
    if (count <= 0)
        return;
    m_selected_index = (m_selected_index + delta + count) % count;
    RefreshSelectionHighlight();
}

void CharacterCreationLayer::RefreshSelectionHighlight()
{
    if (!m_document)
        return;
    const char* list_id = m_step == Step::Class ? "class-list" : "section-list";
    Rml::Element* list = m_document->GetElementById(list_id);
    if (!list)
        return;

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".menu-row");
    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i]->SetClass("selected", static_cast<int>(i) == m_selected_index);
}

void CharacterCreationLayer::ConfirmSelection()
{
    if (m_step == Step::Class)
    {
        if (m_selected_index < 0 || m_selected_index >= static_cast<int>(EnumNames<ClassId>::kValues.size()))
            return;
        m_chosen_class = EnumNames<ClassId>::kValues[static_cast<std::size_t>(m_selected_index)].second;
        ShowSectionStep();
        return;
    }

    if (m_selected_index < 0 || m_selected_index >= static_cast<int>(EnumNames<SectionId>::kValues.size()))
        return;
    const SectionId chosen_section_id = EnumNames<SectionId>::kValues[static_cast<std::size_t>(m_selected_index)].second;
    TransitionTo<GameplayLayer>(m_chosen_class, chosen_section_id);
}

} // namespace psr
