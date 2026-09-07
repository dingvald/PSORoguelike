#pragma once

#include "Engine/Layer.h"
#include "Shop/ShopStock.h"
#include "UI/FieldWidgets.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

struct SDL_Renderer;

namespace Rml {
class ElementDocument;
} // namespace Rml

namespace psr {

class Event;
class RmlClickListener;

// The Shop Stock Editor: edits the hub shop's single fixed catalog
// (App/Assets/Data/shop_stock.json, see Shop/ShopStock.h). Unlike
// PieceEditorLayer/DungeonEditorLayer/AffixEditorLayer's List/Edit shell,
// there's exactly one document here -- no browse mode, just a single
// always-open screen with a reorderable row list (prefab_id_string +
// buy_price per entry, via FieldWidgets' BuildRowList) and a Save button.
// Skips PreviewCanvas/PreviewWindowChrome entirely -- no live-generation
// preview makes sense for a flat price list.
class ShopStockEditorLayer : public Layer
{
public:
    ShopStockEditorLayer();
    ~ShopStockEditorLayer() override;

    ShopStockEditorLayer(const ShopStockEditorLayer&) = delete;
    ShopStockEditorLayer& operator=(const ShopStockEditorLayer&) = delete;
    ShopStockEditorLayer(ShopStockEditorLayer&&) = delete;
    ShopStockEditorLayer& operator=(ShopStockEditorLayer&&) = delete;

    void OnAttach() override;
    void OnDetach() override;
    void OnEvent(Event& event) override;
    void OnRender(SDL_Renderer* renderer) override;

private:
    void LoadDocument();
    void WireButtonClick(const char* element_id, std::function<void()> on_click);
    void ReloadStock();
    void RefreshEntryRows();
    void AddEntry();
    void SaveStock();
    void MarkDirty();
    void RefreshDirtyDisplay();
    void RefreshErrorDisplay();

    Rml::ElementDocument* m_editor = nullptr;
    std::vector<std::unique_ptr<RmlClickListener>> m_listeners; // static toolbar buttons
    fieldwidgets::Listeners m_row_listeners;

    ShopStock m_draft;
    bool m_dirty = false;
    std::string m_error;

    // Deferred row-reorder action, same "apply after this frame's event
    // handling" convention every other row-list editor in this codebase uses
    // (see e.g. DungeonEditorLayer's m_pending_action) -- a drag-reorder
    // callback fires from inside the very row list it would rebuild.
    std::function<void()> m_pending_action;
};

} // namespace psr
