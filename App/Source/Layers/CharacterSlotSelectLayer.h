#pragma once

#include "Engine/Layer.h"

#include <memory>
#include <vector>

namespace Rml {
class ElementDocument;
} // namespace Rml

namespace psr {

class Event;
class KeyPressedEvent;
class RmlClickListener;

// The "Continue" slot picker, reached from MainMenuLayer's Continue row
// (itself only enabled while at least one slot is occupied -- see
// Persistence/CharacterSaveFile.h). Lists kMaxCharacterSaveSlots rows, one
// per slot, dynamically built from ReadCharacterSaveSummary -- same "build
// from data, not hand-typed markup" shape CharacterCreationLayer's own
// class/section-id row lists already use. An occupied slot shows the
// character's name (title) and class/section id/level/meseta (desc); an
// empty slot shows "Empty" and is skipped by keyboard nav / gets no click
// listener, same treatment MainMenuLayer's disabled rows get. Selecting an
// occupied row transitions into GameplayLayer's Continue constructor (see
// GameplayLayer(int save_slot)). Runs entirely before GameplayLayer exists,
// same as CharacterCreationLayer, so this is a plain top-level Layer, not a
// modal GameState.
class CharacterSlotSelectLayer : public Layer
{
public:
    CharacterSlotSelectLayer();
    ~CharacterSlotSelectLayer() override;

    void OnAttach() override;
    void OnDetach() override;
    void OnEvent(Event& event) override;

private:
    bool OnKeyPressed(KeyPressedEvent& event);

    void RefreshSelectionHighlight();
    void MoveSelection(int delta);
    void SelectIndex(int index);
    void ConfirmSelection();

    Rml::ElementDocument* m_document = nullptr;
    int m_selected_index = 0;

    // Parallel to slot index (0..kMaxCharacterSaveSlots) -- which rows
    // MoveSelection/SelectIndex may land on. Computed once in OnAttach from
    // SaveSlotOccupied; this screen isn't reachable at all while there's
    // nothing to continue, so at least one entry is always true.
    std::vector<bool> m_slot_occupied;

    std::vector<std::unique_ptr<RmlClickListener>> m_listeners;
};

} // namespace psr
