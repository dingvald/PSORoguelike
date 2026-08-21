#pragma once

#include "Engine/Math/Vec2.h"
#include "Engine/Render/ZoomLimits.h"

namespace psr {

// The tile position the world renders around. Two modes:
//  - Free (default): Move()/SetPosition() drive it directly -- today's
//    debug-camera behavior (arrow keys / teleport keys).
//  - Tracking (after SetTarget()): the camera adopts whatever Vec2
//    SetTarget() is called with. Callers driving a followed entity (e.g. a
//    future player-follow system) call SetTarget() every time the entity's
//    position changes; Camera itself does not know about entities/ECS.
class Camera
{
public:
    explicit Camera(Vec2 initial_position = {});

    Vec2 GetPosition() const;

    // Free movement. Ignored while HasTarget() -- call ClearTarget() first
    // to regain manual control.
    void Move(int dx, int dy);
    void SetPosition(Vec2 position);

    // Enters/continues tracking: adopts target as the camera position
    // immediately and on every subsequent call.
    void SetTarget(Vec2 target);

    // Leaves tracking; camera stays at the last tracked position until
    // Move()/SetPosition() is called again.
    void ClearTarget();

    bool HasTarget() const;

    // Independent of position/tracking mode. Clamped to
    // [kMinCameraZoom, kMaxCameraZoom] on every call.
    float GetZoom() const;
    void SetZoom(float zoom);

private:
    Vec2 m_position;
    bool m_has_target = false;
    float m_zoom = 1.0f;
};

} // namespace psr
