#include "Layers/CharacterCreationLayer.h"

#include "ApplicationFilepaths.h"
#include "Engine/Events/KeyEvent.h"
#include "Layers/GameplayLayer.h"
#include "Layers/MainMenuLayer.h"
#include "UI/RmlClickListener.h"
#include "UI/RmlEventListener.h"
#include "UI/RmlText.h"

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

#include <array>
#include <cctype>
#include <string>
#include <string_view>
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

CharacterCreationLayer::CharacterCreationLayer(int save_slot) : Layer("CharacterCreationLayer"), m_save_slot(save_slot)
{
}
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
    m_name_change_listener.reset();
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

    // The Name step hands every other key to RmlUi's own text input handling
    // (typing needs Space/Enter to behave as ordinary characters/commit, not
    // as this class's row-list navigation) -- only Escape (back to Section
    // ID) is special-cased here; ConfirmName is reached via the input's own
    // "change" event or the Confirm row's click instead (see ShowNameStep).
    if (m_step == Step::Name)
    {
        if (key_code == SDLK_ESCAPE)
        {
            ShowSectionStep();
            return true;
        }
        return false;
    }

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
    m_name_change_listener.reset();

    if (Rml::Element* class_panel = m_document->GetElementById("class-panel"))
        class_panel->SetProperty("display", "flex");
    if (Rml::Element* section_panel = m_document->GetElementById("section-panel"))
        section_panel->SetProperty("display", "none");
    if (Rml::Element* name_panel = m_document->GetElementById("name-panel"))
        name_panel->SetProperty("display", "none");

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
    m_name_change_listener.reset();

    if (Rml::Element* class_panel = m_document->GetElementById("class-panel"))
        class_panel->SetProperty("display", "none");
    if (Rml::Element* section_panel = m_document->GetElementById("section-panel"))
        section_panel->SetProperty("display", "flex");
    if (Rml::Element* name_panel = m_document->GetElementById("name-panel"))
        name_panel->SetProperty("display", "none");

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

void CharacterCreationLayer::ShowNameStep()
{
    if (!m_document)
        return;
    m_step = Step::Name;
    m_listeners.clear();
    m_name_change_listener.reset();
    m_name.clear();

    if (Rml::Element* class_panel = m_document->GetElementById("class-panel"))
        class_panel->SetProperty("display", "none");
    if (Rml::Element* section_panel = m_document->GetElementById("section-panel"))
        section_panel->SetProperty("display", "none");
    if (Rml::Element* name_panel = m_document->GetElementById("name-panel"))
        name_panel->SetProperty("display", "flex");

    Rml::Element* input = m_document->GetElementById("name-input");
    auto* text_input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(input);
    if (text_input)
    {
        text_input->SetValue("");
        text_input->Focus();

        // RmlUi's text input fires "change" on Enter (as well as on losing
        // focus with a changed value) -- same commit trigger
        // Editor/Source/UI/FieldWidgets.cpp's BuildIntField already relies on.
        m_name_change_listener = std::make_unique<RmlEventListener>(
            "change",
            [this, text_input](Rml::Event&)
            {
                m_name = text_input->GetValue();
                ConfirmName();
            });
        m_name_change_listener->Attach(*input);
    }

    if (Rml::Element* confirm = m_document->GetElementById("name-confirm"))
    {
        auto listener = std::make_unique<RmlClickListener>(
            [this, text_input]
            {
                if (text_input)
                    m_name = text_input->GetValue();
                ConfirmName();
            });
        listener->Attach(*confirm);
        m_listeners.push_back(std::move(listener));
    }
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

    if (m_step == Step::SectionId)
    {
        if (m_selected_index < 0 || m_selected_index >= static_cast<int>(EnumNames<SectionId>::kValues.size()))
            return;
        m_chosen_section_id = EnumNames<SectionId>::kValues[static_cast<std::size_t>(m_selected_index)].second;
        ShowNameStep();
        return;
    }
}

void CharacterCreationLayer::ConfirmName()
{
    const std::string_view whitespace = " \t";
    const std::size_t first = m_name.find_first_not_of(whitespace);
    if (first == std::string::npos)
    {
        m_name.clear();
        return; // whitespace-only/empty -- stay on this step
    }
    const std::size_t last = m_name.find_last_not_of(whitespace);
    m_name = m_name.substr(first, last - first + 1);

    TransitionTo<GameplayLayer>(m_chosen_class, m_chosen_section_id, m_name, m_save_slot);
}

} // namespace psr
