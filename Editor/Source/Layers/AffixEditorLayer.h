#pragma once

#include "Engine/Layer.h"
#include "Items/Affix.h"
#include "Items/AffixLibrary.h"
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

// The Affix Editor: browse/create/delete weapon prefix/suffix definitions
// (see Core/Items/Affix.h) -- a flat, bespoke non-ECS content type,
// so this follows PieceEditorLayer/DungeonEditorLayer's List/Edit shell
// pattern rather than PrefabEditorLayer's entt::meta component-card one.
// Much smaller than either of those: Affix has no nested arrays and no
// spatial/canvas editing, so there's no preview pane and no drag-reorder --
// just four plain fields per affix.
class AffixEditorLayer : public Layer
{
public:
    AffixEditorLayer();
    ~AffixEditorLayer() override;

    AffixEditorLayer(const AffixEditorLayer&) = delete;
    AffixEditorLayer& operator=(const AffixEditorLayer&) = delete;
    AffixEditorLayer(AffixEditorLayer&&) = delete;
    AffixEditorLayer& operator=(AffixEditorLayer&&) = delete;

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
    void ReloadAffixLibrary();
    void RefreshAffixList();
    void OpenForEdit(const std::string& id);
    void BeginNewAffix();
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
    std::vector<std::unique_ptr<RmlClickListener>> m_list_listeners; // rebuildable affix-list rows
    fieldwidgets::Listeners m_form_listeners;

    // -- List state --
    AffixLibrary m_affixes;
    std::string m_pending_delete_id;

    // -- Edit state --
    Affix m_draft;
    std::string m_draft_id;
    std::string m_original_id;
    bool m_is_new = false;
    bool m_dirty = false;
    std::string m_error;
};

} // namespace psr
