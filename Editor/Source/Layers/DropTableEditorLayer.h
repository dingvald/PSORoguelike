#pragma once

#include "Engine/Items/DropTable.h"
#include "Engine/Items/DropTableLibrary.h"
#include "Engine/Layer.h"
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

// The Drop Table Editor: browse/create/delete per-enemy/boss loot tables
// (see Core/Engine/Items/DropTable.h) -- a flat, bespoke non-ECS content
// type, so this follows PhotonArtEditorLayer/AffixEditorLayer's List/Edit
// shell pattern rather than PrefabEditorLayer's entt::meta component-card
// one. Two repeatable entry lists (common_entries/rare_entries), each row a
// three-field {item picker, weight, favored Section ID} -- mirrors
// DungeonEditorLayer's piece-ref rows, including its deferred-reorder
// m_pending_action pattern (see OnRender's doc comment).
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
    void RefreshEntryRows(bool rare);
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
    fieldwidgets::Listeners m_common_row_listeners;
    fieldwidgets::Listeners m_rare_row_listeners;

    // Deferred drag-reorder action, drained in OnRender -- see
    // fieldwidgets::WireDragReorder's doc comment for why a reorder can't
    // rebuild the row list synchronously from the "dragdrop" handler itself.
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
