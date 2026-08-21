#pragma once

#include "Engine/Layer.h"

#include <array>
#include <cstddef>

namespace Rml {
class ElementDocument;
} // namespace Rml

namespace psr {

class Event;
class KeyPressedEvent;

// The Editor's entry-point layer: a small RmlUi main menu that will list
// sub-editors as they're built (Area, Piece, Entity, Item -- see roadmap
// M3.2/M4.2/M5.2/M8.1). For now it has a single "Exit" row; each sub-editor
// milestone appends its own row id + TransitionTo<>() case in
// ConfirmSelection() below.
//
// Keyboard-only for this milestone -- mouse click wiring (RmlClickListener)
// is M2.2 scope, added once there's more than one row worth clicking.
class EditorMenuLayer : public Layer
{
public:
    EditorMenuLayer();

    void OnAttach() override;
    void OnDetach() override;
    void OnEvent(Event& event) override;

private:
    bool OnKeyPressed(KeyPressedEvent& event);

    void RefreshSelectionHighlight();
    void MoveSelection(int delta);
    void ConfirmSelection();

    Rml::ElementDocument* m_document = nullptr;
    int m_selected_index = 0;

    enum Row
    {
        RowExit = 0,
        RowCount = 1
    };
    static constexpr std::array<const char*, RowCount> kRowIds = {"menu-exit"};
};

} // namespace psr
