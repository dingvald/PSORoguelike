#pragma once

#include <SDL3/SDL.h>

namespace psr {

// Shared pan/zoom camera for the SDL/GPU-drawn preview canvas every content
// editor layer (Dungeon/Piece/Prefab) embeds inside its "#grid-panel" RmlUi
// element. Knows nothing about dungeons/pieces/prefabs -- only a world-space
// rectangle (units are whatever the owning layer defines, e.g. pixels at
// zoom 1) and the panel's current screen-space rectangle. Each layer owns one
// as a plain member (composition, same as its TextureAtlas/TileGpuPipeline)
// and routes every screen-space box it draws or hit-tests through it instead
// of computing auto-fit layout math itself.
//
// Wire OnMouseDown/OnMouseMove/OnMouseUp/OnMouseScroll to RmlEventListeners on
// "#edit-body" (mousedown/mousemove/mouseup/mousescroll) -- see
// PieceEditorLayer::WireGridInteraction for the existing listener-ownership
// pattern. Pan is button 2 (middle); left/right (0/1) pass through untouched
// so a layer's own paint/erase listeners on the same element keep working.
class PreviewCanvas
{
public:
    // Call once per frame before drawing/hit-testing, with #grid-panel's
    // current absolute screen box and the content's bounding box in world
    // units. If content_world_bounds differs from the previous call (new
    // content shape/size, first call ever, or ResetView() was requested since
    // the last Update()), the view re-fits: centers content_world_bounds in
    // the panel and picks a covering zoom, clamped to [kMinZoom, kMaxZoom].
    // Otherwise the user's own pan/zoom is left exactly as they set it.
    void Update(const SDL_FRect& panel_screen_rect, const SDL_FRect& content_world_bounds);

    SDL_FRect WorldToScreen(const SDL_FRect& world_rect) const;
    SDL_FPoint ScreenToWorld(SDL_FPoint screen_point) const;

    float GetZoom() const { return m_zoom; }
    int ZoomPercent() const { return static_cast<int>(m_zoom * 100.0f + 0.5f); }

    void OnMouseDown(float screen_x, float screen_y, int button);
    void OnMouseMove(float screen_x, float screen_y);
    void OnMouseUp(int button);
    void OnMouseScroll(float screen_x, float screen_y, float wheel_delta);

    // Steps zoom by one kZoomStep, pivoted on the panel's own center rather
    // than a cursor position -- there is none for a button click. Wired to
    // the preview window's bottom-bar "+"/"-" buttons.
    void ZoomIn();
    void ZoomOut();

    // Re-fits on the next Update() even if content bounds haven't changed --
    // wired to each editor's "Reset View" toolbar button.
    void ResetView() { m_needs_refit = true; }

private:
    bool IsInsidePanel(float screen_x, float screen_y) const;

    SDL_FRect m_panel_rect{};
    SDL_FRect m_last_content_bounds{};
    bool m_fitted = false;
    bool m_needs_refit = false;

    SDL_FPoint m_center_world{};
    float m_zoom = 1.0f;

    bool m_panning = false;
    SDL_FPoint m_pan_last_screen{};
};

} // namespace psr
