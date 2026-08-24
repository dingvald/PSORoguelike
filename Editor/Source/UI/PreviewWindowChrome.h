#pragma once

#include "UI/FieldWidgets.h"

namespace Rml {
class Element;
} // namespace Rml

namespace psr {

class PreviewCanvas;

// Builds the bordered, resizable live-preview chrome every content editor
// (Piece/Dungeon/Prefab) embeds inside its "#preview-window" element -- see
// each layer's RenderEditContent/RenderPreview for the SDL/GPU draw side and
// PreviewCanvas for the pan/zoom math this chrome merely exposes buttons for.
namespace previewwindow {

// `container` must be an existing, empty element (e.g.
// <div id="preview-window" class="preview-window"></div>) placed where a
// bare "#grid-panel" used to sit directly inside ".grid-column". Replaces
// its InnerRML with:
//   - a fresh "#grid-panel"/.grid-panel child -- the same id every layer's
//     render/hit-test code already looks up, unaffected by this change.
//   - a bottom toolbar bar: a zoom-percent readout (id="zoom-readout", so
//     each layer's existing RefreshZoomReadout() needs no changes), a "-"
//     button (PreviewCanvas::ZoomOut), a "+" button (PreviewCanvas::ZoomIn),
//     and a "Center" button (PreviewCanvas::ResetView -- the pre-existing
//     "Reset View" behavior, just relabeled and relocated here).
//   - a bottom-right resize handle that live-adjusts container's own width/
//     height RCSS properties, clamped to a sensible minimum and to
//     container's parent's current box.
//
// preview_canvas must outlive the returned listeners -- same ownership
// contract as every WireButtonClick call site in this codebase: the caller's
// PreviewCanvas member and the returned Listeners vector are both owned by
// the same layer, cleared in OnDetach() before the PreviewCanvas member is
// destroyed.
fieldwidgets::Listeners Build(Rml::Element& container, PreviewCanvas& preview_canvas);

} // namespace previewwindow

} // namespace psr
