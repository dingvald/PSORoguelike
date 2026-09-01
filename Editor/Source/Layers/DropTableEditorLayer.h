#pragma once

#include "Engine/Layer.h"
#include "Items/DropTable.h"
#include "Items/DropTableLibrary.h"
#include "UI/FieldWidgets.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Rml {
class Element;
class ElementDocument;
} // namespace Rml

namespace psr {

class Event;
class RmlClickListener;

// The Drop Table Editor: browse/create/delete per-enemy/boss loot tables (see
// Items/DropTable.h) -- a flat, bespoke non-ECS content type, so this follows
// AffixEditorLayer/PhotonArtEditorLayer's List/Edit shell pattern. Heavier
// than PhotonArtEditorLayer: three repeatable lists (guaranteed drops, common
// entries, rare entries) instead of one, and each common/rare entry carries
// its own collapsed-by-default "Section ID Overrides" sub-panel (reusing
// fieldwidgets::WireCollapseToggle's generic per-item collapse mechanism,
// same one PrefabEditorLayer's Inspector cards use) rather than a dedicated
// 10-column matrix widget, since nothing like that exists in this toolkit and
// most entries never need per-Section-ID favoritism.
class DropTableEditorLayer : public Layer
{
public:
    DropTableEditorLayer();
    ~DropTableEditorLayer() override;

    DropTableEditorLayer(const DropTableEditorLayer&) = delete;
    DropTableEditorLayer& operator=(const DropTableEditorLayer&) = delete;
    DropTableEditorLayer(DropTableEditorLayer&&) = delete;
    DropTableEditorLayer& operator=(DropTableEditorLayer&&) = delete;

    void OnAttach() override;
    void OnDetach() override;
    void OnRender(SDL_Renderer* renderer) override;
    void OnEvent(Event& event) override;

private:
    enum class Mode
    {
        List,
        Edit
    };

    void ShowScreen(Mode mode);

    // -- List mode --
    void ReloadLibrary();
    void RefreshList();
    void OpenForEdit(const std::string& id);
    void BeginNew();
    void RequestDelete(const std::string& id);

    // -- Edit mode --
    void RefreshEditForm();
    void RefreshGuaranteedRows();
    void RefreshEntryRows(Rml::Element& list, std::vector<DropTableEntry>& entries,
                          fieldwidgets::Listeners& row_listeners);
    void MarkDirty();
    void RefreshDirtyDisplay();
    void RefreshErrorDisplay();
    void SaveDraft();

    // -- RmlUi wiring --
    void LoadDocuments();
    void WireButtonClick(const char* element_id, std::function<void()> on_click);

    Mode m_mode = Mode::List;

    Rml::ElementDocument* m_editor = nullptr;
    std::vector<std::unique_ptr<RmlClickListener>> m_listeners;      // static toolbar buttons
    std::vector<std::unique_ptr<RmlClickListener>> m_list_listeners; // rebuildable list rows
    fieldwidgets::Listeners m_form_listeners;
    fieldwidgets::Listeners m_guaranteed_row_listeners;
    fieldwidgets::Listeners m_common_row_listeners;
    fieldwidgets::Listeners m_rare_row_listeners;

    // Reorder (drag-drop) finalizes here, one frame after the drag gesture
    // itself -- see fieldwidgets::WireDragReorder's doc comment, same
    // precedent PhotonArtEditorLayer/PrefabEditorLayer already established.
    // Drained once at the top of OnRender.
    std::function<void()> m_pending_action;

    // -- List state --
    DropTableLibrary m_drop_tables;
    std::string m_pending_delete_id;

    // -- Edit state --
    DropTable m_draft;
    std::string m_draft_id;
    std::string m_original_id;
    bool m_is_new = false;
    bool m_dirty = false;
    std::string m_error;
};

} // namespace psr
