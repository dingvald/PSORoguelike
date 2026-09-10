#pragma once

#include "Areas/Area.h"
#include "Areas/AreaLibrary.h"
#include "Engine/Dungeon/DungeonLibrary.h"
#include "Engine/Layer.h"
#include "UI/FieldPickers.h"
#include "UI/FieldWidgets.h"
#include "UI/InfoPopup.h"
#include "UI/TexturePickerPopup.h"

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

// The Area Editor: browse/create/delete area theme definitions (see
// App/Areas/Area.h) -- a flat, bespoke non-ECS content type, so this follows
// AffixEditorLayer's List/Edit shell pattern rather than PrefabEditorLayer's
// entt::meta component-card one. Area has no spatial/canvas editing -- the
// "tile palette" is three BuildTextureField pickers (floor/wall/accent), the
// same swatch+"Choose..." treatment PrefabEditorLayer's renderable texture_id
// gets, not an inline swatch grid -- so there's no preview pane, matching
// AffixEditorLayer's own scope call. It does have one nested
// array -- dungeon_id_strings (M4.6's ordered per-area dungeon sequence) --
// edited as a reorderable row list, same "load a sibling library read-only
// for the picker, drain a deferred reorder in OnRender" shape
// DungeonEditorLayer's own piece-refs/locks lists already use.
class AreaEditorLayer : public Layer
{
public:
    AreaEditorLayer();
    ~AreaEditorLayer() override;

    AreaEditorLayer(const AreaEditorLayer&) = delete;
    AreaEditorLayer& operator=(const AreaEditorLayer&) = delete;
    AreaEditorLayer(AreaEditorLayer&&) = delete;
    AreaEditorLayer& operator=(AreaEditorLayer&&) = delete;

    void OnAttach() override;
    void OnDetach() override;
    void OnEvent(Event& event) override;
    void OnRender(SDL_Renderer* renderer) override;

private:
    enum class Mode
    {
        List,
        Edit
    };

    void ShowScreen(Mode mode);

    // -- List mode --
    void ReloadAreaLibrary();
    void RefreshAreaList();
    void OpenForEdit(const std::string& id);
    void BeginNewArea();
    void RequestDelete(const std::string& id);

    // -- Edit mode --
    void RefreshEditForm();
    void MarkDirty();
    void RefreshDirtyDisplay();
    void RefreshErrorDisplay();
    void SaveDraft();

    // Rebuilds the dungeon-sequence row list from m_draft.dungeon_id_strings
    // -- one fieldwidgets::BuildRowList row per entry, each a dungeon picker
    // (fieldwidgets::BuildIdEnumField over m_dungeons) that commits by
    // resolving the picked id back to its Dungeon::id_string. Same
    // add/remove/drag-reorder-via-m_pending_action shape as
    // DungeonEditorLayer::RefreshLockRows.
    void RefreshDungeonSequenceRows();

    // -- RmlUi wiring --
    void LoadDocuments();
    void WireButtonClick(const char* element_id, std::function<void()> on_click);

    Mode m_mode = Mode::List;

    Rml::ElementDocument* m_editor = nullptr;
    Rml::ElementDocument* m_info_popup_document = nullptr;
    Rml::ElementDocument* m_texture_picker_document = nullptr;
    InfoPopup m_info_popup;
    TexturePickerPopup m_texture_picker;
    FieldPickers m_pickers;
    std::vector<std::unique_ptr<RmlClickListener>> m_listeners;      // static toolbar buttons
    std::vector<std::unique_ptr<RmlClickListener>> m_list_listeners; // rebuildable area-list rows
    fieldwidgets::Listeners m_card_listeners; // static dungeon-sequence inspector-card collapse toggle
    fieldwidgets::Listeners m_form_listeners;

    // -- List state --
    AreaLibrary m_areas;
    std::string m_pending_delete_id;

    // Read-only, loaded alongside m_areas -- feeds the dungeon-sequence
    // row list's id picker only; never written back.
    DungeonLibrary m_dungeons;

    // -- Edit state --
    Area m_draft;
    std::string m_draft_id;
    std::string m_original_id;
    bool m_is_new = false;
    bool m_dirty = false;
    std::string m_error;

    fieldwidgets::Listeners m_dungeon_row_listeners;

    // Drained once at the top of OnRender -- see
    // fieldwidgets::WireDragReorder's doc comment for why a reorder request
    // can't rebuild the row list synchronously from inside its own
    // "dragdrop" handler.
    std::function<void()> m_pending_action;
};

} // namespace psr
