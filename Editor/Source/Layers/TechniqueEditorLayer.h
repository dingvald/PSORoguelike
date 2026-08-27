#pragma once

#include "Engine/Combat/Technique.h"
#include "Engine/Combat/TechniqueLibrary.h"
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

// The Technique Editor: browse/create/delete Wand/Cane-attached Technique
// definitions (see Core/Engine/Combat/Technique.h) -- mirrors
// PhotonArtEditorLayer's shape exactly (a flat, bespoke non-ECS content type
// following AffixEditorLayer's List/Edit shell pattern; tp_cost is shared
// with PhotonArt -- both spend the same TPComponent pool), differing only in
// which struct fields the form exposes (element_id instead of
// hits_per_turn/drain_percent).
class TechniqueEditorLayer : public Layer
{
public:
    TechniqueEditorLayer();
    ~TechniqueEditorLayer() override;

    TechniqueEditorLayer(const TechniqueEditorLayer&) = delete;
    TechniqueEditorLayer& operator=(const TechniqueEditorLayer&) = delete;
    TechniqueEditorLayer(TechniqueEditorLayer&&) = delete;
    TechniqueEditorLayer& operator=(TechniqueEditorLayer&&) = delete;

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
    void RefreshTierRows();
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
    fieldwidgets::Listeners m_tier_row_listeners;

    // Reorder (drag-drop) finalizes here, one frame after the drag gesture
    // itself, rather than synchronously from inside the "dragdrop" handler --
    // see fieldwidgets::WireDragReorder's doc comment for why. Drained once
    // at the top of OnRender.
    std::function<void()> m_pending_action;

    // -- List state --
    TechniqueLibrary m_techniques;
    std::string m_pending_delete_id;

    // -- Edit state --
    Technique m_draft;
    std::string m_draft_id;
    std::string m_original_id;
    bool m_is_new = false;
    bool m_dirty = false;
    std::string m_error;
};

} // namespace psr
