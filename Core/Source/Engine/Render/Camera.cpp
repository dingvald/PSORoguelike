#include "Engine/Render/Camera.h"

#include <cmath>

namespace psr {

namespace {
    // Exponential decay rate for the render lag: at this rate the lag drops
    // to ~5% of its post-jump size in about a quarter-second, fast enough to
    // read as "smooth follow" rather than "sluggish" on a per-tile grid step.
    constexpr float kFollowDecayRate = 12.0f;
} // namespace

Camera::Camera(Vec2 initial_position) : m_position(initial_position) {}

Vec2 Camera::GetPosition() const { return m_position; }

Vec2f Camera::GetRenderOffset() const { return m_render_lag; }

void Camera::Reposition(Vec2 new_position)
{
    // Keep the rendered position (m_position + m_render_lag) continuous
    // across the jump: fold the jump into the lag instead of resetting it,
    // so Update() eases away the whole accumulated distance, not just the
    // latest step.
    m_render_lag = m_render_lag + Vec2f{static_cast<float>(m_position.x - new_position.x),
                                       static_cast<float>(m_position.y - new_position.y)};
    m_position = new_position;
}

void Camera::Move(int dx, int dy)
{
    if (m_has_target)
        return;

    Reposition(Vec2{m_position.x + dx, m_position.y + dy});
}

void Camera::SetPosition(Vec2 position)
{
    if (m_has_target)
        return;

    Reposition(position);
}

void Camera::SetTarget(Vec2 target)
{
    m_has_target = true;
    Reposition(target);
}

void Camera::ClearTarget() { m_has_target = false; }

bool Camera::HasTarget() const { return m_has_target; }

void Camera::Update(float delta_time)
{
    const float decay = std::exp(-kFollowDecayRate * delta_time);
    m_render_lag = m_render_lag * decay;
}

float Camera::GetZoom() const { return m_zoom; }

void Camera::SetZoom(float zoom) { m_zoom = ClampCameraZoom(zoom); }

} // namespace psr
