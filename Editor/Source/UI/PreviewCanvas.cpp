#include "UI/PreviewCanvas.h"

#include <algorithm>
#include <cmath>

namespace psr {

namespace {

// Screen-pixels-per-world-unit bounds -- distinct from Core's ZoomLimits.h,
// which clamps the gameplay camera's flat tile-multiplier zoom, a different
// unit system entirely. Generous enough that fitting a large generated
// dungeon isn't clipped and zooming into a single prefab sprite stays useful.
constexpr float kMinZoom = 0.1f;
constexpr float kMaxZoom = 8.0f;

// Multiplicative step per wheel notch.
constexpr float kZoomStep = 1.15f;

constexpr float kBoundsEpsilon = 0.01f;

bool NearlyEqual(float a, float b) { return std::abs(a - b) <= kBoundsEpsilon; }

} // namespace

void PreviewCanvas::Update(const SDL_FRect& panel_screen_rect, const SDL_FRect& content_world_bounds)
{
    m_panel_rect = panel_screen_rect;

    const bool bounds_changed =
        !m_fitted || !NearlyEqual(content_world_bounds.x, m_last_content_bounds.x) ||
        !NearlyEqual(content_world_bounds.y, m_last_content_bounds.y) ||
        !NearlyEqual(content_world_bounds.w, m_last_content_bounds.w) ||
        !NearlyEqual(content_world_bounds.h, m_last_content_bounds.h);

    if (!bounds_changed && !m_needs_refit)
        return;

    m_last_content_bounds = content_world_bounds;
    m_needs_refit = false;
    m_fitted = true;

    if (content_world_bounds.w <= 0.0f || content_world_bounds.h <= 0.0f || panel_screen_rect.w <= 0.0f ||
        panel_screen_rect.h <= 0.0f)
        return;

    m_zoom = std::clamp(std::min(panel_screen_rect.w / content_world_bounds.w,
                                  panel_screen_rect.h / content_world_bounds.h),
                         kMinZoom, kMaxZoom);
    m_center_world = SDL_FPoint{content_world_bounds.x + content_world_bounds.w * 0.5f,
                                 content_world_bounds.y + content_world_bounds.h * 0.5f};
}

SDL_FRect PreviewCanvas::WorldToScreen(const SDL_FRect& world_rect) const
{
    const SDL_FPoint panel_center{m_panel_rect.x + m_panel_rect.w * 0.5f, m_panel_rect.y + m_panel_rect.h * 0.5f};
    return SDL_FRect{panel_center.x + (world_rect.x - m_center_world.x) * m_zoom,
                      panel_center.y + (world_rect.y - m_center_world.y) * m_zoom, world_rect.w * m_zoom,
                      world_rect.h * m_zoom};
}

SDL_FPoint PreviewCanvas::ScreenToWorld(SDL_FPoint screen_point) const
{
    const SDL_FPoint panel_center{m_panel_rect.x + m_panel_rect.w * 0.5f, m_panel_rect.y + m_panel_rect.h * 0.5f};
    return SDL_FPoint{m_center_world.x + (screen_point.x - panel_center.x) / m_zoom,
                       m_center_world.y + (screen_point.y - panel_center.y) / m_zoom};
}

void PreviewCanvas::ZoomIn()
{
    m_zoom = std::clamp(m_zoom * kZoomStep, kMinZoom, kMaxZoom);
}

void PreviewCanvas::ZoomOut()
{
    m_zoom = std::clamp(m_zoom / kZoomStep, kMinZoom, kMaxZoom);
}

bool PreviewCanvas::IsInsidePanel(float screen_x, float screen_y) const
{
    return screen_x >= m_panel_rect.x && screen_x <= m_panel_rect.x + m_panel_rect.w && screen_y >= m_panel_rect.y &&
           screen_y <= m_panel_rect.y + m_panel_rect.h;
}

void PreviewCanvas::OnMouseDown(float screen_x, float screen_y, int button)
{
    if (button != 2 || !IsInsidePanel(screen_x, screen_y))
        return;
    m_panning = true;
    m_pan_last_screen = SDL_FPoint{screen_x, screen_y};
}

void PreviewCanvas::OnMouseMove(float screen_x, float screen_y)
{
    if (!m_panning)
        return;
    m_center_world.x -= (screen_x - m_pan_last_screen.x) / m_zoom;
    m_center_world.y -= (screen_y - m_pan_last_screen.y) / m_zoom;
    m_pan_last_screen = SDL_FPoint{screen_x, screen_y};
}

void PreviewCanvas::OnMouseUp(int button)
{
    if (button == 2)
        m_panning = false;
}

void PreviewCanvas::OnMouseScroll(float screen_x, float screen_y, float wheel_delta)
{
    if (wheel_delta == 0.0f || !IsInsidePanel(screen_x, screen_y))
        return;

    const SDL_FPoint world_before = ScreenToWorld(SDL_FPoint{screen_x, screen_y});

    // RmlUi's mousescroll wheel_delta is positive "down" (scroll-down
    // convention) -- scrolling down (positive) zooms out, up (negative) zooms
    // in, matching most pan/zoom canvases.
    const float factor = wheel_delta < 0.0f ? kZoomStep : 1.0f / kZoomStep;
    m_zoom = std::clamp(m_zoom * factor, kMinZoom, kMaxZoom);

    // Re-solve the center so world_before stays under the cursor.
    const SDL_FPoint panel_center{m_panel_rect.x + m_panel_rect.w * 0.5f, m_panel_rect.y + m_panel_rect.h * 0.5f};
    m_center_world.x = world_before.x - (screen_x - panel_center.x) / m_zoom;
    m_center_world.y = world_before.y - (screen_y - panel_center.y) / m_zoom;
}

} // namespace psr
