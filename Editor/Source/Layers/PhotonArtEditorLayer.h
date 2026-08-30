#pragma once

#include "Combat/PhotonArt.h"
#include "Combat/PhotonArtLibrary.h"
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

// The Photon Art Editor: browse/create/delete weapon-attached Photon Art
// definitions (see Core/Combat/PhotonArt.h) -- a flat, bespoke
// non-ECS content type, so this follows AffixEditorLayer/PieceEditorLayer's
// List/Edit shell pattern rather than PrefabEditorLayer's entt::meta
// component-card one. Heavier than AffixEditorLayer (more scalar fields plus
// a repeatable tiers list), but no preview canvas/spatial editing.
class PhotonArtEditorLayer : public Layer
{
public:
    PhotonArtEditorLayer();
    ~PhotonArtEditorLayer() override;

    PhotonArtEditorLayer(const PhotonArtEditorLayer&) = delete;
    PhotonArtEditorLayer& operator=(const PhotonArtEditorLayer&) = delete;
    PhotonArtEditorLayer(PhotonArtEditorLayer&&) = delete;
    PhotonArtEditorLayer& operator=(PhotonArtEditorLayer&&) = delete;

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
    PhotonArtLibrary m_photon_arts;
    std::string m_pending_delete_id;

    // -- Edit state --
    PhotonArt m_draft;
    std::string m_draft_id;
    std::string m_original_id;
    bool m_is_new = false;
    bool m_dirty = false;
    std::string m_error;
};

} // namespace psr
