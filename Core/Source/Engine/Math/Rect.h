#pragma once

#include "Engine/Math/Vec2.h"

#include <algorithm>

namespace psr {

// An axis-aligned integer rect: [origin, origin+size), half-open. No
// validation on construction (mirrors Vec2's own minimal style); Empty() is
// the caller's job to check once size may have gone non-positive (e.g. after
// Intersect or a negative Inset).
struct Rect
{
    Vec2 origin;
    Vec2 size;

    friend bool operator==(const Rect&, const Rect&) = default;

    int Left() const { return origin.x; }
    int Top() const { return origin.y; }
    int Right() const { return origin.x + size.x; }
    int Bottom() const { return origin.y + size.y; }
    bool Empty() const { return size.x <= 0 || size.y <= 0; }

    bool Contains(Vec2 point) const
    {
        return point.x >= Left() && point.x < Right() && point.y >= Top() && point.y < Bottom();
    }

    Rect Intersect(const Rect& other) const
    {
        const int left = std::max(Left(), other.Left());
        const int top = std::max(Top(), other.Top());
        const int right = std::min(Right(), other.Right());
        const int bottom = std::min(Bottom(), other.Bottom());
        return Rect{{left, top}, {right - left, bottom - top}};
    }

    // Positive n shrinks the rect by n on every side; negative n grows it by
    // |n| on every side.
    Rect Inset(int n) const { return Rect{origin + Vec2{n, n}, size - Vec2{2 * n, 2 * n}}; }
};

} // namespace psr
