#pragma once

#include "Engine/Layer.h"

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

namespace Rml {
class ElementDocument;
} // namespace Rml

namespace psr {

class Event;
class KeyPressedEvent;
class RmlClickListener;

// The Editor's entry-point layer: a small RmlUi main menu that will list
// sub-editors as they're built (Area, Piece, Entity, Item -- see roadmap
// M3.2/M4.2/M5.2/M8.1). Each sub-editor milestone appends its own row id +
// TransitionTo<>() case in ConfirmSelection() below -- Pieces (M4.2) is the
// first. Prefabs is the generic, stats-free entity-prefab editor (an early
// slice of M5.2, ahead of the M5.1 stat components). Affixes (M8.1) is the
// bespoke weapon prefix/suffix library, separate from Prefabs since an Affix
// isn't an entity -- weapon/armor/mod authoring itself stayed folded into
// the Prefabs row (see PrefabEditorLayer's class doc comment). Status effects
// are authored directly as JSON (Core/Combat/StatusEffect.h) -- just
// four plain fields, not worth a dedicated editor.
//
// Both keyboard (Up/Down/Enter/Space) and mouse (hover selects, click
// confirms, via RmlClickListener) drive the same MoveSelection()/
// ConfirmSelection() pair -- theme.rcss's .menu-row:hover rule was already
// giving every row a hover affordance despite no click ever being wired up.
class EditorMenuLayer : public Layer
{
public:
    EditorMenuLayer();
    ~EditorMenuLayer() override;

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
    std::vector<std::unique_ptr<RmlClickListener>> m_listeners;

    enum Row
    {
        RowPieces = 0,
        RowDungeons = 1,
        RowPrefabs = 2,
        RowAffixes = 3,
        RowPhotonArts = 4,
        RowTechniques = 5,
        RowExit = 6,
        RowCount = 7
    };
    static constexpr std::array<const char*, RowCount> kRowIds = {"menu-pieces",  "menu-dungeons",    "menu-prefabs",
                                                                  "menu-affixes", "menu-photon-arts", "menu-techniques",
                                                                  "menu-exit"};
};

} // namespace psr
