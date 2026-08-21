#pragma once

#include "Engine/Math/Color.h"
#include "UI/FieldWidgets.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace Rml {
class ElementDocument;
class Element;
class ElementFormControlInput;
} // namespace Rml

namespace psr {

// A full-color-space picker popup: a saturation/value square (two stacked
// layers of solid-colour strips -- white->hue horizontal, transparent->black
// vertical, the standard two-gradient SV-square technique, but built from
// discrete `background-color` fills rather than `linear-gradient` decorators;
// see RefreshSvGradient's doc comment for why) with a draggable indicator, a
// hue strip (the same discrete-strip technique, one rainbow's worth of rows)
// with its own draggable indicator, and an alpha slider (RmlUi's native
// <input type="range">). Dragging uses RmlUi's native drag/dragstart/dragend
// events, not document-wide mouse tracking -- a drag stays captured to its
// source element even once the pointer leaves its bounds.
//
// Bound against an already-loaded document rather than loading its own -- the
// owning editor layer owns all of its RmlUi documents and calls Bind() once
// after loading this popup's document.
class ColorPickerPopup
{
public:
    void Bind(Rml::ElementDocument& document);
    void Unbind(); // drop listeners while the document's elements are still alive

    // Shows the popup preloaded with initial. on_pick fires on every live edit
    // (drag move, alpha slide, hex edit) -- not just once on close -- so the
    // field that opened the picker can keep its swatch live-updated while the
    // user drags.
    void Open(Color initial, std::function<void(Color)> on_pick);
    void Close();
    bool IsOpen() const { return m_open; }

private:
    void RefreshFromHsva();   // recomputes the RGBA color from h/s/v/a and notifies on_pick
    void RefreshIndicators(); // repositions the sv/hue indicator elements

    // Recolours the SV-square's saturation columns for the current hue
    // (building them once on first call). This stands in for a
    // `linear-gradient` decorator, which this project's SDL RenderInterface
    // can't render (no CompileShader support) -- so the white->hue horizontal
    // gradient is approximated as a row of solid-colour column elements
    // instead. The vertical transparent->black half of the classic
    // two-gradient technique is BuildValueOverlayRows below, layered on top;
    // it doesn't depend on hue, so it's only built once.
    void RefreshSvGradient();
    void BuildValueOverlayRows(); // static transparent->black alpha ramp, built once in Bind()
    void BuildHueGradientRows();  // static rainbow strip for #hue-strip, built once in Bind()

    void HandleSvPointer(float window_x, float window_y);
    void HandleHuePointer(float window_y);
    void HandleHexCommit();

    Rml::ElementDocument* m_document = nullptr;
    Rml::Element* m_sv_square = nullptr;
    Rml::Element* m_sv_saturation_layer = nullptr;
    Rml::Element* m_sv_value_layer = nullptr;
    Rml::Element* m_hue_strip = nullptr;
    Rml::Element* m_hue_gradient = nullptr;
    Rml::Element* m_sv_indicator = nullptr;
    Rml::Element* m_hue_indicator = nullptr;
    Rml::ElementFormControlInput* m_alpha_slider = nullptr;
    Rml::ElementFormControlInput* m_hex_input = nullptr;

    // Cached column elements inside m_sv_saturation_layer, built once by
    // RefreshSvGradient on its first call and recoloured (never rebuilt)
    // afterward.
    std::vector<Rml::Element*> m_sv_saturation_columns;

    float m_hue = 0.0f;        // [0, 360)
    float m_saturation = 0.0f; // [0, 1]
    float m_value = 0.0f;      // [0, 1]
    std::uint8_t m_alpha = 255;

    bool m_open = false;
    std::function<void(Color)> m_on_pick;

    fieldwidgets::Listeners m_listeners;
};

} // namespace psr
