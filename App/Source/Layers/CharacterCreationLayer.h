#pragma once

#include "Engine/Layer.h"
#include "Items/SectionId.h"
#include "Progression/CharacterClass.h"

#include <memory>
#include <vector>

namespace Rml {
class ElementDocument;
} // namespace Rml

namespace psr {

class Event;
class KeyPressedEvent;
class RmlClickListener;

// Character creation: two sequential row-list steps in one document (Class,
// then Section ID), each dynamically built from EnumNames<ClassId>/
// EnumNames<SectionId> (not hand-typed markup) so the row list can never
// drift from the fixed enum roster it represents. Confirming the Section ID
// step is the final confirmation -- transitions straight into
// TransitionTo<GameplayLayer>(chosen_class, chosen_section_id). Runs entirely
// before GameplayLayer exists, so this is a plain top-level Layer, not a
// modal GameState.
class CharacterCreationLayer : public Layer
{
public:
    CharacterCreationLayer();
    ~CharacterCreationLayer() override;

    void OnAttach() override;
    void OnDetach() override;
    void OnEvent(Event& event) override;

private:
    enum class Step
    {
        Class,
        SectionId
    };

    bool OnKeyPressed(KeyPressedEvent& event);

    void ShowClassStep();
    void ShowSectionStep();

    void MoveSelection(int delta);
    void RefreshSelectionHighlight();
    void ConfirmSelection();

    Rml::ElementDocument* m_document = nullptr;
    Step m_step = Step::Class;
    int m_selected_index = 0;
    ClassId m_chosen_class = ClassId::Hunter;
    std::vector<std::unique_ptr<RmlClickListener>> m_listeners;
};

} // namespace psr
