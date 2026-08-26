#pragma once

#include "Engine/ECS/Registry.h"

namespace psr {

// Advances every entity's in-flight TweenComponent by delta_time, removing
// it once it completes. Pure logic over Registry -- no rendering, no SDL.
void UpdateTweens(Registry& registry, float delta_time);

} // namespace psr
