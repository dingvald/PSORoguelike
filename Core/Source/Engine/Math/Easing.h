#pragma once

namespace psr {

// Minimal easing curves for tile-fraction render offsets (see TweenComponent
// in App) -- t is expected in [0, 1]; callers clamp before calling.
inline float EaseOutQuad(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }

inline float EaseInQuad(float t) { return t * t; }

} // namespace psr
