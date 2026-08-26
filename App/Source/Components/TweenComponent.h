#pragma once

#include "Engine/Math/Vec2f.h"

namespace psr {

// Transient render-only state: an in-flight sub-tile render offset easing
// toward {0,0} (see RegistryRenderableLookup::GetRenderOffset), driven by
// TweenSystem. Deliberately NOT meta-registered -- never stamped via prefab,
// never cloned/described -- this is purely engine-internal per-frame state,
// not content data.
struct TweenComponent
{
    Vec2f start_offset; // render offset at t=0, in tile-fraction units
    float duration = 0.0f;
    float elapsed = 0.0f;
};

} // namespace psr
