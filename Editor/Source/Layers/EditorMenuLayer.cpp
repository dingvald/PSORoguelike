#include "Layers/EditorMenuLayer.h"

#include "Engine/Events/KeyEvent.h"
#include "Layers/AffixEditorLayer.h"
#include "Layers/DropTableEditorLayer.h"
#include "Layers/DungeonEditorLayer.h"
#include "Layers/PieceEditorLayer.h"
#include "Layers/PrefabEditorLayer.h"
#include "UI/RmlClickListener.h"

#include <EditorFilepaths.h>

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

namespace psr {

namespace {
    std::filesystem::path FontPath = EditorFilepaths::FontsPath / "PixelCode-Regular.ttf";
    std::filesystem::path FontPathBold = EditorFilepaths::FontsPath / "PixelCode-Bold.ttf";
    std::filesystem::path DocumentPath = EditorFilepaths::RmlDocumentsPath / "editor_menu.rml";
} // namespace

EditorMenuLayer::EditorMenuLayer() : Layer("EditorMenuLayer") {}
EditorMenuLayer::~EditorMenuLayer() = default;

void EditorMenuLayer::OnAttach()
{
    if (!Rml::LoadFontFace(FontPath.string().c_str()))
        SDL_Log("Warning: EditorMenuLayer failed to load font '%s'", FontPath.string().c_str());
    if (!Rml::LoadFontFace(FontPathBold.string().c_str()))
        SDL_Log("Warning: EditorMenuLayer failed to load font '%s'", FontPathBold.string().c_str());

    GuiContext::LockedAccess gui_context = GetLockedGuiContext();
    m_document = gui_context->LoadDocument(DocumentPath.string().c_str());
    if (!m_document)
    {
        SDL_Log("Warning: EditorMenuLayer has no document to show");
        return;
    }

    m_selected_index = RowPieces;
    RefreshSelectionHighlight();

    for (std::size_t i = 0; i < kRowIds.size(); ++i)
    {
        Rml::Element* row = m_document->GetElementById(kRowIds[i]);
        if (!row)
            continue;
        const int index = static_cast<int>(i);
        auto listener = std::make_unique<RmlClickListener>([this, index] { SelectIndex(index); });
        listener->Attach(*row);
        m_listeners.push_back(std::move(listener));
    }

    m_document->Show();
}

void EditorMenuLayer::OnDetach()
{
    m_listeners.clear();
    if (m_document)
    {
        m_document->Close();
        m_document = nullptr;
    }
}

void EditorMenuLayer::OnEvent(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) { return OnKeyPressed(e); });
}

bool EditorMenuLayer::OnKeyPressed(KeyPressedEvent& event)
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
    return false;
}

void EditorMenuLayer::RefreshSelectionHighlight()
{
    if (!m_document)
        return;
    for (std::size_t i = 0; i < kRowIds.size(); ++i)
        if (Rml::Element* row = m_document->GetElementById(kRowIds[i]))
            row->SetClass("selected", static_cast<int>(i) == m_selected_index);
}

void EditorMenuLayer::MoveSelection(int delta)
{
    m_selected_index = (m_selected_index + delta + RowCount) % RowCount;
    RefreshSelectionHighlight();
}

void EditorMenuLayer::SelectIndex(int index)
{
    m_selected_index = index;
    RefreshSelectionHighlight();
    ConfirmSelection();
}

void EditorMenuLayer::ConfirmSelection()
{
    switch (m_selected_index)
    {
    case RowPieces:
        TransitionTo<PieceEditorLayer>();
        break;
    case RowDungeons:
        TransitionTo<DungeonEditorLayer>();
        break;
    case RowPrefabs:
        TransitionTo<PrefabEditorLayer>();
        break;
    case RowAffixes:
        TransitionTo<AffixEditorLayer>();
        break;
    case RowDropTables:
        TransitionTo<DropTableEditorLayer>();
        break;
    case RowExit:
        RequestQuit();
        break;
    default:
        break;
    }
}

} // namespace psr
