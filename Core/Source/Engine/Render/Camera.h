#pragma once

#include "Engine/Math/Vec2.h"
#include "Engine/Math/Vec2f.h"
#include "Engine/Render/ZoomLimits.h"

namespace psr {

// The tile position the world renders around. Two modes:
//  - Free (default): Move()/SetPosition() drive it directly -- today's
//    debug-camera behavior (arrow keys / teleport keys).
//  - Tracking (after SetTarget()): the camera adopts whatever Vec2
//    SetTarget() is called with. Callers driving a followed entity (e.g.
//    GameplayLayer's own player-follow) call SetTarget() every time the
//    entity's position changes; Camera itself does not know about
//    entities/ECS.
//
// GetPosition() is the *logical* tile -- exact and immediate, updated the
// same call SetTarget()/Move()/SetPosition() makes (existing callers depend
// on that; see CameraTests.cpp). The rendered camera instead eases toward
// it: every reposition banks the jump as a render lag (tile-fraction units),
// which Update() decays toward {0,0} once per frame; GetRenderOffset()
// exposes the remainder for TileRenderer to fold into its pixel math, the
// same way a TweenComponent's offset already does per-entity. Because
// SetTarget() only ever receives the followed entity's grid Position (never
// a TweenComponent render offset -- see GameplayLayer::OnUpdate), an
// attack's lunge tween never feeds into this smoothing.
class Camera
{
public:
    explicit Camera(Vec2 initial_position = {});

    Vec2 GetPosition() const;

    // Tile-fraction render lag behind GetPosition(), eased toward {0,0} by
    // Update(). {0,0} until the first reposition.
    Vec2f GetRenderOffset() const;

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

    // Decays the render lag toward {0,0} at a fixed exponential rate,
    // independent of frame rate. Call once per frame.
    void Update(float delta_time);

    // Independent of position/tracking mode. Clamped to
    // [kMinCameraZoom, kMaxCameraZoom] on every call.
    float GetZoom() const;
    void SetZoom(float zoom);

private:
    void Reposition(Vec2 new_position);

    Vec2 m_position;
    Vec2f m_render_lag;
    bool m_has_target = false;
    float m_zoom = 1.0f;
};

} // namespace psr
