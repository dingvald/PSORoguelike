#pragma once

#include "Engine/ECS/Registry.h"

namespace psr {

// Advances every entity's in-flight TweenComponent queue by delta_time,
// carrying leftover time across queued Tweens within the same call (so one
// large delta_time can finish an entire queue and fire every completion in
// one call), removing the component once its queue drains empty. Pure logic
// over Registry -- no rendering, no SDL.
void UpdateTweens(Registry& registry, float delta_time);

} // namespace psr
