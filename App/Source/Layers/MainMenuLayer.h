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
// Continue/Options/Credits have no backing system yet (save/load is M11.2,
// the options screen is M15.4, real credits content is M14.6) -- Continue is
// disabled outright (skipped by keyboard nav, no click listener attached);
// Options/Credits are selectable but only ever show an inert "Coming Soon"
// placeholder panel with a Back row.
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
    static constexpr std::array<bool, RowCount> kRowEnabled = {false, true, true, true, true};
};

} // namespace psr
