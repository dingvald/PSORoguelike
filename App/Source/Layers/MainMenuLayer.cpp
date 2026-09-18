#include "Layers/MainMenuLayer.h"

#include "ApplicationFilepaths.h"
#include "Engine/Events/KeyEvent.h"
#include "Layers/CharacterCreationLayer.h"
#include "Layers/CharacterSlotSelectLayer.h"
#include "Persistence/CharacterSaveFile.h"
#include "UI/RmlClickListener.h"

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

#include <optional>

namespace psr {

namespace {
    std::filesystem::path FontPath = ApplicationFilepaths::FontsPath / "PixelCode-Regular.ttf";
    std::filesystem::path FontPathBold = ApplicationFilepaths::FontsPath / "PixelCode-Bold.ttf";
    std::filesystem::path DocumentPath = ApplicationFilepaths::RmlDocumentsPath / "main_menu.rml";
} // namespace

MainMenuLayer::MainMenuLayer() : Layer("MainMenuLayer") {}
MainMenuLayer::~MainMenuLayer() = default;

void MainMenuLayer::OnAttach()
{
    if (!Rml::LoadFontFace(FontPath.string().c_str()))
        SDL_Log("Warning: MainMenuLayer failed to load font '%s'", FontPath.string().c_str());
    if (!Rml::LoadFontFace(FontPathBold.string().c_str()))
        SDL_Log("Warning: MainMenuLayer failed to load font '%s'", FontPathBold.string().c_str());

    GuiContext::LockedAccess gui_context = GetLockedGuiContext();
    m_document = gui_context->LoadDocument(DocumentPath.string().c_str());
    if (!m_document)
    {
        SDL_Log("Warning: MainMenuLayer has no document to show");
        return;
    }

    // Recomputed every time this layer is (re-)attached -- coming back here
    // from Quit to Title after playing (or just having created a character
    // for the first time) can change whether Continue has anything to offer.
    m_row_enabled[RowContinue] = false;
    for (int slot = 0; slot < kMaxCharacterSaveSlots; ++slot)
        if (SaveSlotOccupied(slot))
        {
            m_row_enabled[RowContinue] = true;
            break;
        }

    m_selected_index = m_row_enabled[RowContinue] ? RowContinue : RowNewCharacter;
    RefreshSelectionHighlight();

    for (std::size_t i = 0; i < kRowIds.size(); ++i)
    {
        Rml::Element* row = m_document->GetElementById(kRowIds[i]);
        if (!row)
            continue;

        // RowContinue's "disabled" class is hardcoded in main_menu.rml (the
        // common case -- a fresh install has no save yet) but m_row_enabled
        // is recomputed above every time this layer attaches, so the visual
        // state has to be kept in sync here rather than trusted from markup.
        row->SetClass("disabled", !m_row_enabled[i]);
        if (!m_row_enabled[i])
            continue;

        const int index = static_cast<int>(i);
        auto listener = std::make_unique<RmlClickListener>([this, index] { SelectIndex(index); });
        listener->Attach(*row);
        m_listeners.push_back(std::move(listener));
    }

    if (Rml::Element* back = m_document->GetElementById("placeholder-back"))
    {
        auto listener = std::make_unique<RmlClickListener>([this] { HidePlaceholder(); });
        listener->Attach(*back);
        m_listeners.push_back(std::move(listener));
    }

    m_document->Show();
}

void MainMenuLayer::OnDetach()
{
    m_listeners.clear();
    if (m_document)
    {
        m_document->Close();
        m_document = nullptr;
    }
}

void MainMenuLayer::OnEvent(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) { return OnKeyPressed(e); });
}

bool MainMenuLayer::OnKeyPressed(KeyPressedEvent& event)
{
    if (event.IsRepeat())
        return false;

    const int key_code = event.GetKeyCode();

    if (m_placeholder_open)
    {
        if (key_code == SDLK_ESCAPE || key_code == SDLK_RETURN || key_code == SDLK_KP_ENTER ||
            key_code == SDLK_SPACE)
        {
            HidePlaceholder();
            return true;
        }
        return true; // swallow everything else while the placeholder is up
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
    return false;
}

void MainMenuLayer::RefreshSelectionHighlight()
{
    if (!m_document)
        return;
    for (std::size_t i = 0; i < kRowIds.size(); ++i)
        if (Rml::Element* row = m_document->GetElementById(kRowIds[i]))
            row->SetClass("selected", static_cast<int>(i) == m_selected_index);
}

void MainMenuLayer::MoveSelection(int delta)
{
    int next = m_selected_index;
    for (int step = 0; step < RowCount; ++step)
    {
        next = (next + delta + RowCount) % RowCount;
        if (m_row_enabled[static_cast<std::size_t>(next)])
            break;
    }
    m_selected_index = next;
    RefreshSelectionHighlight();
}

void MainMenuLayer::SelectIndex(int index)
{
    if (index < 0 || index >= RowCount || !m_row_enabled[static_cast<std::size_t>(index)])
        return;
    m_selected_index = index;
    RefreshSelectionHighlight();
    ConfirmSelection();
}

void MainMenuLayer::ConfirmSelection()
{
    switch (m_selected_index)
    {
    case RowContinue:
        TransitionTo<CharacterSlotSelectLayer>();
        break;
    case RowNewCharacter:
        if (std::optional<int> slot = FindFirstEmptySaveSlot())
            TransitionTo<CharacterCreationLayer>(*slot);
        else
            ShowPlaceholder("All character slots are full");
        break;
    case RowOptions:
        ShowPlaceholder("Options");
        break;
    case RowCredits:
        ShowPlaceholder("Credits");
        break;
    case RowQuit:
        RequestQuit();
        break;
    default:
        break;
    }
}

void MainMenuLayer::ShowPlaceholder(const char* title)
{
    if (!m_document)
        return;
    if (Rml::Element* menu_panel = m_document->GetElementById("menu-panel"))
        menu_panel->SetProperty("display", "none");
    if (Rml::Element* placeholder_title = m_document->GetElementById("placeholder-title"))
        placeholder_title->SetInnerRML(title);
    if (Rml::Element* placeholder_panel = m_document->GetElementById("placeholder-panel"))
        placeholder_panel->SetProperty("display", "flex");
    m_placeholder_open = true;
}

void MainMenuLayer::HidePlaceholder()
{
    if (!m_document)
        return;
    if (Rml::Element* placeholder_panel = m_document->GetElementById("placeholder-panel"))
        placeholder_panel->SetProperty("display", "none");
    if (Rml::Element* menu_panel = m_document->GetElementById("menu-panel"))
        menu_panel->SetProperty("display", "flex");
    m_placeholder_open = false;
}

} // namespace psr
