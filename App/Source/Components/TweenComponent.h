#pragma once

#include "Engine/Math/Vec2f.h"

#include <functional>
#include <vector>

namespace psr {

// One in-flight render-offset interpolation: eases from start_offset to
// end_offset over duration, in tile-fraction units (see
// RegistryRenderableLookup::GetRenderOffset). on_completion, if set, fires
// exactly once when this Tween finishes -- e.g. AttackAction uses it to defer
// damage application until a lunge-toward-the-target Tween completes.
struct Tween
{
    Vec2f start_offset;
    Vec2f end_offset;
    float duration = 0.0f;
    float elapsed = 0.0f;
    std::function<void()> on_completion;
};

// A FIFO queue of Tweens on one entity, driven by TweenSystem -- only
// queue.front() is ever the active/rendered one; it's popped (firing its
// on_completion) once finished, and the next queued Tween takes over. The
// component itself is removed once the queue drains empty. Deliberately NOT
// meta-registered -- never stamped via prefab, never cloned/described -- this
// is purely engine-internal per-frame state, not content data.
struct TweenComponent
{
    std::vector<Tween> queue;
};

} // namespace psr
