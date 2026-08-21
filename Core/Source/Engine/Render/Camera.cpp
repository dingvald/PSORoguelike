#include "Engine/Render/Camera.h"

namespace psr {

Camera::Camera(Vec2 initial_position) : m_position(initial_position) {}

Vec2 Camera::GetPosition() const { return m_position; }

void Camera::Move(int dx, int dy)
{
    if (m_has_target)
        return;

    m_position.x += dx;
    m_position.y += dy;
}

void Camera::SetPosition(Vec2 position)
{
    if (m_has_target)
        return;

    m_position = position;
}

void Camera::SetTarget(Vec2 target)
{
    m_has_target = true;
    m_position = target;
}

void Camera::ClearTarget() { m_has_target = false; }

bool Camera::HasTarget() const { return m_has_target; }

float Camera::GetZoom() const { return m_zoom; }

void Camera::SetZoom(float zoom) { m_zoom = ClampCameraZoom(zoom); }

} // namespace psr
