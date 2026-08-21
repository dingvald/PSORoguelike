#pragma once

#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
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
    Listeners WireCollapseToggle(Rml::Element& item);

} // namespace fieldwidgets

} // namespace psr
