#pragma once

#include "Engine/Layer.h"
#include "Items/SectionId.h"
#include "Progression/CharacterClass.h"

#include <memory>
#include <string>
#include <vector>

namespace Rml {
class ElementDocument;
} // namespace Rml

namespace psr {

class Event;
class KeyPressedEvent;
class RmlClickListener;
class RmlEventListener;

// Character creation: Class, then Section ID (two sequential row-list steps,
// each dynamically built from EnumNames<ClassId>/EnumNames<SectionId> -- not
// hand-typed markup -- so the row list can never drift from the fixed enum
// roster it represents), then a Name step (a plain RmlUi text input, same
// <input type="text">/"change"-event pattern Editor/Source/UI/FieldWidgets.cpp
// already uses). Confirming the Name step is the final confirmation --
// transitions straight into TransitionTo<GameplayLayer>(chosen_class,
// chosen_section_id, name, save_slot). save_slot is which slot the new
// character will occupy -- picked by MainMenuLayer's FindFirstEmptySaveSlot
// before this layer is even pushed, just threaded through here to hand off at
// the end. Runs entirely before GameplayLayer exists, so this is a plain
// top-level Layer, not a modal GameState.
class CharacterCreationLayer : public Layer
{
public:
    explicit CharacterCreationLayer(int save_slot);
    ~CharacterCreationLayer() override;

    void OnAttach() override;
    void OnDetach() override;
    void OnEvent(Event& event) override;

private:
    enum class Step
    {
        Class,
        SectionId,
        Name
    };

    bool OnKeyPressed(KeyPressedEvent& event);

    void ShowClassStep();
    void ShowSectionStep();
    void ShowNameStep();

    void MoveSelection(int delta);
    void RefreshSelectionHighlight();
    void ConfirmSelection();

    // Trims m_name and, if non-empty, transitions into GameplayLayer with all
    // three character-creation picks. A no-op (stays on the Name step) while
    // m_name is empty/whitespace-only -- there's no separate validation
    // message UI yet, so an empty confirm just silently does nothing.
    void ConfirmName();

    int m_save_slot;
    Rml::ElementDocument* m_document = nullptr;
    Step m_step = Step::Class;
    int m_selected_index = 0;
    ClassId m_chosen_class = ClassId::Hunter;
    SectionId m_chosen_section_id = SectionId::Viridia;
    std::string m_name;
    std::vector<std::unique_ptr<RmlClickListener>> m_listeners;

    // Separate from m_listeners (which ShowClassStep/ShowSectionStep/
    // ShowNameStep all clear when transitioning between steps) since the
    // Name step's text input listener is a different RmlUi listener type
    // (RmlEventListener, not RmlClickListener) -- see ShowNameStep.
    std::unique_ptr<RmlEventListener> m_name_change_listener;
};

} // namespace psr
