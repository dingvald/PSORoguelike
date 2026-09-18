#pragma once

#include "Engine/Layer.h"

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

namespace Rml {
class ElementDocument;
class Element;
} // namespace Rml

namespace psr {

class Event;
class KeyPressedEvent;
class RmlClickListener;

// The title screen -- App's entry-point layer (see main.cpp), pushed instead
// of GameplayLayer so a player lands somewhere before the turn loop starts.
// Shape mirrors Editor/Source/Layers/EditorMenuLayer.h almost directly:
// keyboard (Up/Down/Enter/Space) and mouse (RmlClickListener) both drive the
// same MoveSelection()/ConfirmSelection() pair. No shared app-level
// Title/Playing/Paused state machine exists yet -- which top-level Layer is
// currently attached already fully encodes Title vs. Playing, and Paused
// (M14.2, not yet built) is the first state that would actually need one,
// since it requires suspending GameplayLayer rather than replacing it.
//
// Options/Credits have no backing system yet (the options screen is M15.4,
// real credits content is M14.6) -- selectable, but only ever show an inert
// "Coming Soon" placeholder panel with a Back row. Continue/New Character are
// both save-slot-aware (see Persistence/CharacterSaveFile.h): Continue is
// enabled only while at least one slot is occupied (OnAttach recomputes
// m_row_enabled each time this layer is shown, since a slot may have been
// filled or emptied since); New Character finds the first empty slot itself
// (FindFirstEmptySaveSlot) rather than asking the player to pick one, and
// falls back to an "All character slots are full" placeholder (no delete-save
// UI exists yet -- M14.3's own future work) if every slot is occupied.
class MainMenuLayer : public Layer
{
public:
    MainMenuLayer();
    ~MainMenuLayer() override;

    void OnAttach() override;
    void OnDetach() override;
    void OnEvent(Event& event) override;

private:
    bool OnKeyPressed(KeyPressedEvent& event);

    void RefreshSelectionHighlight();
    void MoveSelection(int delta);
    void SelectIndex(int index);
    void ConfirmSelection();

    void ShowPlaceholder(const char* title);
    void HidePlaceholder();

    Rml::ElementDocument* m_document = nullptr;
    int m_selected_index = 0;
    bool m_placeholder_open = false;
    std::vector<std::unique_ptr<RmlClickListener>> m_listeners;

    enum Row
    {
        RowContinue = 0,
        RowNewCharacter = 1,
        RowOptions = 2,
        RowCredits = 3,
        RowQuit = 4,
        RowCount = 5
    };
    static constexpr std::array<const char*, RowCount> kRowIds = {
        "menu-continue", "menu-new-character", "menu-options", "menu-credits", "menu-quit"};

    // RowContinue's slot is recomputed in OnAttach (SaveSlotOccupied); every
    // other row is unconditionally selectable.
    std::array<bool, RowCount> m_row_enabled = {false, true, true, true, true};
};

} // namespace psr
