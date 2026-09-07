#include "Layers/ShopStockEditorLayer.h"

#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Layers/EditorMenuLayer.h"
#include "Shop/ShopStockFile.h"
#include "UI/RmlClickListener.h"
#include "UI/RmlText.h"

#include <EditorFilepaths.h>

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

#include <cstddef>
#include <utility>

namespace psr {

namespace {
    const std::filesystem::path kFontPath = EditorFilepaths::FontsPath / "PixelCode-Regular.ttf";
    const std::filesystem::path kFontPathBold = EditorFilepaths::FontsPath / "PixelCode-Bold.ttf";
    const std::filesystem::path kEditorDocument = EditorFilepaths::RmlDocumentsPath / "shop_stock_editor.rml";
} // namespace

ShopStockEditorLayer::ShopStockEditorLayer() : Layer("ShopStockEditorLayer") {}
ShopStockEditorLayer::~ShopStockEditorLayer() = default;

void ShopStockEditorLayer::OnAttach()
{
    if (!Rml::LoadFontFace(kFontPath.string().c_str()))
        SDL_Log("Warning: ShopStockEditorLayer failed to load font '%s'", kFontPath.string().c_str());
    if (!Rml::LoadFontFace(kFontPathBold.string().c_str()))
        SDL_Log("Warning: ShopStockEditorLayer failed to load font '%s'", kFontPathBold.string().c_str());

    LoadDocument();
    ReloadStock();
    RefreshEntryRows();
}

void ShopStockEditorLayer::OnDetach()
{
    m_row_listeners.clear();
    m_listeners.clear();

    if (m_editor)
    {
        m_editor->Close();
        m_editor = nullptr;
    }
}

void ShopStockEditorLayer::OnRender(SDL_Renderer* /*renderer*/)
{
    // Drains a reorder requested by fieldwidgets::WireDragReorder, if any --
    // see BuildRowList's own doc comment for why this can't run
    // synchronously from the "dragdrop" handler itself, same idiom every
    // other row-list editor in this codebase uses.
    if (m_pending_action)
    {
        const std::function<void()> action = std::exchange(m_pending_action, nullptr);
        action();
    }
}

void ShopStockEditorLayer::OnEvent(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>(
        [this](KeyPressedEvent& e)
        {
            if (e.GetKeyCode() != SDLK_ESCAPE)
                return false;
            TransitionTo<EditorMenuLayer>();
            return true;
        });
}

void ShopStockEditorLayer::LoadDocument()
{
    {
        GuiContext::LockedAccess gui_context = GetLockedGuiContext();
        m_editor = gui_context->LoadDocument(kEditorDocument.string().c_str());
    }
    if (!m_editor)
    {
        SDL_Log("Warning: ShopStockEditorLayer has no editor document");
        return;
    }

    WireButtonClick("add-entry", [this] { AddEntry(); });
    WireButtonClick("save-stock", [this] { SaveStock(); });
    WireButtonClick("back-to-menu", [this] { TransitionTo<EditorMenuLayer>(); });

    m_editor->Show();
}

void ShopStockEditorLayer::WireButtonClick(const char* element_id, std::function<void()> on_click)
{
    if (!m_editor)
        return;
    Rml::Element* element = m_editor->GetElementById(element_id);
    if (!element)
        return;
    auto listener = std::make_unique<RmlClickListener>(std::move(on_click));
    listener->Attach(*element);
    m_listeners.push_back(std::move(listener));
}

void ShopStockEditorLayer::ReloadStock()
{
    try
    {
        m_draft = LoadShopStock(EditorFilepaths::ShopStockPath);
        m_error.clear();
    }
    catch (const std::exception& error)
    {
        m_draft = ShopStock{};
        m_error = error.what();
    }
    m_dirty = false;
    RefreshErrorDisplay();
    RefreshDirtyDisplay();
}

void ShopStockEditorLayer::RefreshEntryRows()
{
    if (!m_editor)
        return;
    m_row_listeners.clear();

    Rml::Element* list = m_editor->GetElementById("shop-stock-entry-list");
    if (!list)
        return;

    const std::vector<std::string> content(
        m_draft.entries.size(),
        "<div class=\"entry-prefab-id field-row\"></div><div class=\"entry-buy-price field-row\"></div>");

    fieldwidgets::RowList result = fieldwidgets::BuildRowList(
        *list, content, "<div class=\"list-empty\">No entries yet -- click Add Entry to create one.</div>",
        [this](std::size_t index)
        {
            if (index < m_draft.entries.size())
                m_draft.entries.erase(m_draft.entries.begin() + static_cast<std::ptrdiff_t>(index));
            MarkDirty();
            RefreshEntryRows();
        },
        [this](std::size_t from, std::size_t to)
        {
            m_pending_action = [this, from, to]
            {
                fieldwidgets::MoveElement(m_draft.entries, from, to);
                MarkDirty();
                RefreshEntryRows();
            };
        });

    for (std::size_t i = 0; i < result.rows.size() && i < m_draft.entries.size(); ++i)
    {
        const std::size_t index = i;
        if (Rml::Element* row = result.rows[i]->QuerySelector(".entry-prefab-id"))
            for (auto& listener :
                 fieldwidgets::BuildStringField(*row, "prefab_id_string", m_draft.entries[i].prefab_id_string,
                                                [this, index](std::string text)
                                                {
                                                    if (index < m_draft.entries.size())
                                                        m_draft.entries[index].prefab_id_string = std::move(text);
                                                    MarkDirty();
                                                }))
                m_row_listeners.push_back(std::move(listener));
        if (Rml::Element* row = result.rows[i]->QuerySelector(".entry-buy-price"))
            for (auto& listener :
                 fieldwidgets::BuildIntField(*row, "buy_price", m_draft.entries[i].buy_price,
                                             [this, index](int value)
                                             {
                                                 if (index < m_draft.entries.size())
                                                     m_draft.entries[index].buy_price = value;
                                                 MarkDirty();
                                             }))
                m_row_listeners.push_back(std::move(listener));
    }
}

void ShopStockEditorLayer::AddEntry()
{
    m_draft.entries.push_back(ShopStockEntry{});
    MarkDirty();
    RefreshEntryRows();
}

void ShopStockEditorLayer::SaveStock()
{
    try
    {
        SaveShopStock(EditorFilepaths::ShopStockPath, m_draft);
        m_error.clear();
        m_dirty = false;
    }
    catch (const std::exception& error)
    {
        m_error = error.what();
        SDL_Log("ShopStockEditorLayer: save failed: %s", m_error.c_str());
    }
    RefreshErrorDisplay();
    RefreshDirtyDisplay();
}

void ShopStockEditorLayer::MarkDirty()
{
    m_dirty = true;
    RefreshDirtyDisplay();
}

void ShopStockEditorLayer::RefreshDirtyDisplay()
{
    if (!m_editor)
        return;
    if (Rml::Element* dirty = m_editor->GetElementById("stock-dirty"))
        dirty->SetInnerRML(m_dirty ? "*unsaved*" : "");
}

void ShopStockEditorLayer::RefreshErrorDisplay()
{
    if (!m_editor)
        return;
    if (Rml::Element* error = m_editor->GetElementById("stock-error"))
        error->SetInnerRML(EscapeRml(m_error));
}

} // namespace psr
