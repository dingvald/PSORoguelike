#include "UI/PreviewWindowChrome.h"

#include "UI/PreviewCanvas.h"
#include "UI/RmlClickListener.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>

#include <algorithm>
#include <string>

namespace psr::previewwindow {

namespace {

constexpr float kMinPreviewWidth = 160.0f;
constexpr float kMinPreviewHeight = 120.0f;

constexpr const char* kBodyHtml = R"(
<div id="grid-panel" class="grid-panel"></div>
<div class="preview-toolbar">
    <span class="btn preview-zoom-out">-</span>
    <span id="zoom-readout" class="text-accent">100%</span>
    <span class="btn preview-zoom-in">+</span>
    <span class="btn preview-center">Center</span>
</div>
<div class="preview-resize-handle"></div>
)";

// Grows/shrinks `container` from its fixed top-left corner to follow the
// drag's absolute pointer position, clamped to a minimum size and to
// however much room is left in container's parent (".grid-column").
void HandleResizeDrag(Rml::Element& container, Rml::Event& event)
{
    Rml::Element* parent = container.GetParentNode();
    if (!parent)
        return;

    const Rml::Vector2f container_offset = container.GetAbsoluteOffset();
    const Rml::Vector2f parent_offset = parent->GetAbsoluteOffset();
    const Rml::Vector2f parent_size = parent->GetBox().GetSize();

    const float mouse_x = event.GetParameter<float>("mouse_x", 0.0f);
    const float mouse_y = event.GetParameter<float>("mouse_y", 0.0f);

    const float max_width = std::max(kMinPreviewWidth, parent_offset.x + parent_size.x - container_offset.x);
    const float max_height = std::max(kMinPreviewHeight, parent_offset.y + parent_size.y - container_offset.y);

    const float new_width = std::clamp(mouse_x - container_offset.x, kMinPreviewWidth, max_width);
    const float new_height = std::clamp(mouse_y - container_offset.y, kMinPreviewHeight, max_height);

    container.SetProperty("width", std::to_string(new_width) + "px");
    container.SetProperty("height", std::to_string(new_height) + "px");
}

} // namespace

fieldwidgets::Listeners Build(Rml::Element& container, PreviewCanvas& preview_canvas)
{
    fieldwidgets::Listeners listeners;

    container.SetInnerRML(kBodyHtml);

    if (Rml::Element* zoom_out = container.QuerySelector(".preview-zoom-out"))
    {
        auto listener = std::make_unique<RmlClickListener>([&preview_canvas] { preview_canvas.ZoomOut(); });
        listener->Attach(*zoom_out);
        listeners.push_back(std::move(listener));
    }
    if (Rml::Element* zoom_in = container.QuerySelector(".preview-zoom-in"))
    {
        auto listener = std::make_unique<RmlClickListener>([&preview_canvas] { preview_canvas.ZoomIn(); });
        listener->Attach(*zoom_in);
        listeners.push_back(std::move(listener));
    }
    if (Rml::Element* center = container.QuerySelector(".preview-center"))
    {
        auto listener = std::make_unique<RmlClickListener>([&preview_canvas] { preview_canvas.ResetView(); });
        listener->Attach(*center);
        listeners.push_back(std::move(listener));
    }

    if (Rml::Element* handle = container.QuerySelector(".preview-resize-handle"))
    {
        // The handle sits inside "#edit-body", which every editor layer
        // already wires mousedown/mousemove/mouseup to PreviewCanvas pan and
        // (for Piece) paint-cell hit-testing. Without stopping propagation, a
        // resize-drag's initial mousedown would also bubble to #edit-body's
        // handler -- for Piece, painting/erasing the cell under the handle
        // while the user is trying to resize instead.
        auto mouse_down = std::make_unique<RmlEventListener>("mousedown",
                                                              [](Rml::Event& event) { event.StopPropagation(); });
        mouse_down->Attach(*handle);
        listeners.push_back(std::move(mouse_down));

        // dragstart/drag is RmlUi's real pointer-capture drag (continues
        // following the pointer even once it leaves the handle's own tiny
        // bounds) -- see ColorPickerPopup's #sv-square/#hue-strip for the
        // same precedent.
        for (const char* event_name : {"dragstart", "drag"})
        {
            auto listener = std::make_unique<RmlEventListener>(
                event_name,
                [&container](Rml::Event& event)
                {
                    HandleResizeDrag(container, event);
                    event.StopPropagation();
                });
            listener->Attach(*handle);
            listeners.push_back(std::move(listener));
        }
    }

    return listeners;
}

} // namespace psr::previewwindow
