#pragma once

#include "Areas/Area.h"
#include "Areas/AreaLibrary.h"
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

// The Area Editor: browse/create/delete area theme definitions (see
// App/Areas/Area.h) -- a flat, bespoke non-ECS content type, so this follows
// AffixEditorLayer's List/Edit shell pattern rather than PrefabEditorLayer's
// entt::meta component-card one. Area has no nested arrays and no spatial/
// canvas editing (the "tile palette" is three plain texture-id fields, not a
// picker-backed swatch grid -- see AreaEditorLayer.cpp's own note), so
// there's no preview pane and no drag-reorder, matching AffixEditorLayer's
// own scope call.
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

    // -- RmlUi wiring --
    void LoadDocuments();
    void WireButtonClick(const char* element_id, std::function<void()> on_click);

    Mode m_mode = Mode::List;

    Rml::ElementDocument* m_editor = nullptr;
    std::vector<std::unique_ptr<RmlClickListener>> m_listeners;      // static toolbar buttons
    std::vector<std::unique_ptr<RmlClickListener>> m_list_listeners; // rebuildable area-list rows
    fieldwidgets::Listeners m_form_listeners;

    // -- List state --
    AreaLibrary m_areas;
    std::string m_pending_delete_id;

    // -- Edit state --
    Area m_draft;
    std::string m_draft_id;
    std::string m_original_id;
    bool m_is_new = false;
    bool m_dirty = false;
    std::string m_error;
};

} // namespace psr
