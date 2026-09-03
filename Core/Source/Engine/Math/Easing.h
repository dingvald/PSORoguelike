#pragma once

namespace psr {

// Minimal easing curves for tile-fraction render offsets (see TweenComponent
// in App) -- t is expected in [0, 1]; callers clamp before calling.
inline float EaseOutQuad(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }

inline float EaseInQuad(float t) { return t * t; }

inline float Linear(float t) { return t; }

enum class EasingCurve
{
    Linear,
    EaseOutQuad,
    EaseInQuad
};

inline float Ease(EasingCurve curve, float t)
{
    switch (curve)
    {
    case EasingCurve::EaseOutQuad:
        return EaseOutQuad(t);
    case EasingCurve::EaseInQuad:
        return EaseInQuad(t);
    case EasingCurve::Linear:
        return Linear(t);
    }
    return Linear(t);
}

} // namespace psr
