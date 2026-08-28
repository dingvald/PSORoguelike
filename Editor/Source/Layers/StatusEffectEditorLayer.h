#pragma once

#include "Engine/Combat/StatusEffect.h"
#include "Engine/Combat/StatusEffectLibrary.h"
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

// The Status Effect Editor: browse/create/delete Poison/Burn/Freeze/Shock/
// Confuse ailment definitions (see Core/Engine/Combat/StatusEffect.h) -- a
// flat, bespoke non-ECS content type, so this follows AffixEditorLayer's
// List/Edit shell pattern exactly (no nested arrays, no spatial/canvas
// editing -- just four plain fields per status effect).
class StatusEffectEditorLayer : public Layer
{
public:
    StatusEffectEditorLayer();
    ~StatusEffectEditorLayer() override;

    StatusEffectEditorLayer(const StatusEffectEditorLayer&) = delete;
    StatusEffectEditorLayer& operator=(const StatusEffectEditorLayer&) = delete;
    StatusEffectEditorLayer(StatusEffectEditorLayer&&) = delete;
    StatusEffectEditorLayer& operator=(StatusEffectEditorLayer&&) = delete;

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
    void ReloadStatusEffectLibrary();
    void RefreshStatusEffectList();
    void OpenForEdit(const std::string& id);
    void BeginNewStatusEffect();
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
    std::vector<std::unique_ptr<RmlClickListener>> m_list_listeners; // rebuildable status-effect-list rows
    fieldwidgets::Listeners m_form_listeners;

    // -- List state --
    StatusEffectLibrary m_status_effects;
    std::string m_pending_delete_id;

    // -- Edit state --
    StatusEffect m_draft;
    std::string m_draft_id;
    std::string m_original_id;
    bool m_is_new = false;
    bool m_dirty = false;
    std::string m_error;
};

} // namespace psr
