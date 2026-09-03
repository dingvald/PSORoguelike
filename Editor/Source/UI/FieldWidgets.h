#pragma once

#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Rml {
class Element;
class EventListener;
} // namespace Rml

namespace psr {

// One row-builder per content-schema field kind, used to render a sub-editor's
// schema-informed field form. Each function fills `row` (an existing,
// already-in-the-document container element -- e.g. a freshly SetInnerRML'd
// "<div class=\"field-row\"></div>") with a label plus one or more value
// controls, wires live-commit listener(s) for it, and returns those listeners
// for the caller to keep alive (mirrors the RmlClickListener ownership
// pattern -- the owning layer stores these for the row's lifetime).
//
// Fields commit on blur/Enter (RmlUi's default "change" event timing for
// <input>), not on every keystroke. Invalid input (fails to parse as the
// field's type) gets an "invalid" CSS class and is not committed -- the field
// simply keeps showing the offending text until it's corrected or reverted.
//
// This toolkit only knows how to build/wire widgets against getter/setter
// callbacks -- it has no knowledge of any specific content schema.
//
// No BuildTilePositionField here (unlike the sibling toolkit this was ported
// from): TilePosition is a World-specific type that doesn't exist in this
// project yet (see roadmap M3). It slots in alongside that milestone the same
// way M1.2 already deferred TilePosition support elsewhere in the ECS.
namespace fieldwidgets {

    using Listeners = std::vector<std::unique_ptr<Rml::EventListener>>;

    Listeners BuildIntField(Rml::Element& row, const std::string& label, int initial,
                            std::function<void(int)> on_commit);

    Listeners BuildFloatField(Rml::Element& row, const std::string& label, float initial,
                              std::function<void(float)> on_commit);

    Listeners BuildStringField(Rml::Element& row, const std::string& label, const std::string& initial,
                               std::function<void(std::string)> on_commit);

    Listeners BuildBoolField(Rml::Element& row, const std::string& label, bool initial,
                             std::function<void(bool)> on_commit);

    // A name-as-id field: a literal integer commits the raw id; any other text
    // hashes via entt::hashed_string::value, the same convention JsonEntityLoader
    // uses. initial_text is shown verbatim if non-empty (the authored name, so
    // the field is editable as the string it was authored as, not the hash it
    // evaluates to); otherwise falls back to initial_id's decimal form. on_commit
    // receives both the parsed id and the raw text committed (empty when the
    // user typed a bare integer -- there is no name to retain in that case), so a
    // caller with a string sibling to a hashed id field can keep it in sync;
    // callers without one simply ignore the second parameter.
    Listeners BuildNameIdField(Rml::Element& row, const std::string& label, std::uint32_t initial_id,
                               const std::string& initial_text,
                               std::function<void(std::uint32_t, std::string)> on_commit);

    Listeners BuildVec2Field(Rml::Element& row, const std::string& label, Vec2 initial,
                             std::function<void(Vec2)> on_commit);

    // A dropdown restricted to options, via RmlUi's native <select>.
    Listeners BuildEnumField(Rml::Element& row, const std::string& label, const std::vector<std::string>& options,
                             const std::string& initial, std::function<void(std::string)> on_commit);

    // BuildEnumField's sibling for option sets keyed by something other than
    // their own display text -- e.g. picking an entry out of a NameId-indexed
    // library (pieces, prefabs, ...) by its authored name while committing
    // the hashed id, the same way BuildNameIdField's typed-text field does.
    // options pairs are (id, display label); the label is escaped internally,
    // so callers pass raw authored text. If initial_id doesn't match any
    // option's id (e.g. a stale/unauthored reference), nothing is selected.
    Listeners BuildIdEnumField(Rml::Element& row, const std::string& label,
                               const std::vector<std::pair<std::uint32_t, std::string>>& options,
                               std::uint32_t initial_id, std::function<void(std::uint32_t)> on_commit);

    // A hex text field + live swatch + "Pick..." button. on_open_picker is called
    // with the field's current value and a callback to invoke with the picked
    // result (see ColorPickerPopup) -- this toolkit doesn't own the picker popup.
    Listeners BuildColorField(Rml::Element& row, const std::string& label, Color initial,
                              std::function<void(Color)> on_commit,
                              std::function<void(Color, std::function<void(Color)>)> on_open_picker);

    // A swatch + "Choose..." button for a texture-id field (in place of a plain
    // NameId box). initial_text is the authored texture stem name if known, shown
    // in place of the raw hash (see BuildNameIdField). on_commit receives the
    // picked stem's id and name; on_open_picker mirrors BuildColorField's.
    Listeners BuildTextureField(
        Rml::Element& row, const std::string& label, std::uint32_t initial_id, const std::string& initial_text,
        std::function<void(std::uint32_t, std::string)> on_commit,
        std::function<void(std::uint32_t, std::function<void(std::uint32_t, std::string)>)> on_open_picker);

    // Wires a collapse/expand caret for one rebuildable list entry: item must
    // contain a ".collapse-toggle" child (the caret itself) -- clicking it
    // toggles item's "collapsed" CSS class and flips the caret glyph. The
    // stylesheet is what actually hides the entry's body -- this only flips the
    // class. No-op (returns empty) if item has no ".collapse-toggle" child.
    // Collapse state is purely a view concern and isn't preserved across a full
    // rebuild of item's parent list.
    //
    // use_chevron picks the glyph pair: false (default) is PrefabEditorLayer's
    // original "+"/"-", kept as-is for those pre-existing cards; true is
    // ">"/"v", used by every new collapsible-card call site
    // (BuildCardList below, and any static per-screen card wired directly with
    // this function) -- both are plain ASCII for the same reason the original
    // comment gave: the editor's pixel font isn't guaranteed to have Unicode
    // geometric-shape/triangle codepoints.
    Listeners WireCollapseToggle(Rml::Element& item, bool use_chevron = false);

    // Wires native RmlUi drag-and-drop reordering across an already-built row
    // set: rows[i]/handles[i] must be parallel (same length, same order).
    // handles[i] (expected to carry RCSS `drag: drag;`, e.g. a ".drag-handle"
    // child of rows[i]) is the drag source; rows[i] itself (expected to carry
    // `drag: drag-drop;`) is the drop target that receives "dragover"/
    // "dragdrop". Dropping handle i onto row j calls request_reorder(i, j)
    // once, on "dragdrop".
    //
    // request_reorder must ONLY stash the two indices for later (e.g. into a
    // layer's deferred m_pending_action, drained on a later OnRender) -- it
    // must never rebuild/destroy the row list synchronously. RmlClickListener
    // above documents that ElementDocument::Close() is deferred because
    // RmlUi's update loop can't safely have elements destroyed into it
    // mid-dispatch; the same hazard applies here, since rows/handles are
    // still live RmlUi elements mid-drag when "dragdrop" fires.
    Listeners WireDragReorder(const std::vector<Rml::Element*>& rows, const std::vector<Rml::Element*>& handles,
                              std::function<void(std::size_t from_index, std::size_t to_index)> request_reorder);

    // One rebuilt addable/removable row list, shared by every editor screen
    // that has one (Dungeon's piece-refs/locks, Prefab's socket tags, Piece's
    // per-cell stamped prefabs, ...) instead of each screen hand-rolling its
    // own markup-loop + remove-button wiring. Rebuilds container with one
    // ".row-card" per entry in content_html (each string is that row's
    // caller-specific inner fields -- e.g. what today's per-editor code
    // already builds by hand), wrapped in the shared drag-handle + remove-
    // button chrome (see field_widgets.rcss's .row-card), or empty_message if
    // content_html is empty. Wires the remove button (calls on_remove(index))
    // and drag-reorder (via WireDragReorder above, calls
    // request_reorder(from, to) -- same deferred-only constraint applies).
    //
    // Returns the row elements so the caller can QuerySelector() its own
    // field containers out of each row by class and wire them, exactly as
    // every editor's row-list code already does today.
    struct RowList
    {
        Listeners listeners;
        std::vector<Rml::Element*> rows;
    };
    RowList BuildRowList(Rml::Element& container, const std::vector<std::string>& content_html,
                         const std::string& empty_message, std::function<void(std::size_t)> on_remove,
                         std::function<void(std::size_t, std::size_t)> request_reorder);

    // BuildRowList's sibling for entries with enough fields that showing them
    // all inline would overwhelm the sidebar -- the fuller header+chevron+body
    // "inspector card" chrome PrefabEditorLayer's component cards established
    // (generalized here so other rebuildable lists, e.g. Dungeon's piece-refs/
    // locks, can look and behave the same way instead of hand-rolling their own
    // card loop; Prefab's own cards predate this helper and aren't ported to it
    // -- they keep their own "+"/"-" glyph). Rebuilds container with one
    // ".inspector-card.list-item" per entry in summaries/bodies (parallel
    // arrays, same length): summaries[i] is the card header's label (plain
    // text/markup -- e.g. a resolved display name -- not itself an editable
    // field; if the field that determines it can change, the caller re-queries
    // cards[i]->QuerySelector(".component-title") to update it live, same as
    // Dungeon's piece-ref/lock cards do for their id/type fields), and
    // bodies[i] is the collapsible body's inner markup (the caller's own
    // field-row containers, wired exactly as BuildRowList's rows are). Cards
    // start collapsed, using the ">"/"v" chevron glyph (WireCollapseToggle's
    // use_chevron=true). Wires collapse, remove (on_remove(index)), and
    // drag-reorder (request_reorder, same deferred-only constraint as
    // WireDragReorder/BuildRowList).
    struct CardList
    {
        Listeners listeners;
        std::vector<Rml::Element*> cards;
    };
    CardList BuildCardList(Rml::Element& container, const std::vector<std::string>& summaries,
                           const std::vector<std::string>& bodies, const std::string& empty_message,
                           std::function<void(std::size_t)> on_remove,
                           std::function<void(std::size_t, std::size_t)> request_reorder);

    // Shared index math for every request_reorder callback (WireDragReorder/
    // BuildRowList's contract): moves the element at from_index to
    // to_index's ORIGINAL (pre-move) position -- i.e. dropping row i onto row
    // j places it immediately where j used to be, shifting elements between
    // them by one. No-op if either index is out of range or they're equal.
    template <typename T>
    void MoveElement(std::vector<T>& items, std::size_t from_index, std::size_t to_index)
    {
        if (from_index >= items.size() || to_index >= items.size() || from_index == to_index)
            return;
        T value = std::move(items[from_index]);
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(from_index));
        const std::size_t insert_at = from_index < to_index ? to_index - 1 : to_index;
        items.insert(items.begin() + static_cast<std::ptrdiff_t>(insert_at), std::move(value));
    }

} // namespace fieldwidgets

} // namespace psr
