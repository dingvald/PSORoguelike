#pragma once

#include <algorithm>

namespace psr {

// Shared camera zoom bounds -- kept in one place so every zoomable view
// agrees on the same range.
inline constexpr float kMinCameraZoom = 1.0f;
inline constexpr float kMaxCameraZoom = 4.0f;

inline constexpr float ClampCameraZoom(float zoom) { return std::clamp(zoom, kMinCameraZoom, kMaxCameraZoom); }

} // namespace psr
