#pragma once

#include "Engine/Math/Color.h"

namespace psr {

// Runtime-only "this entity is mid damage-flash" marker, stamped by
// DamageEffectSystem::OnDamage and driven every frame by
// DamageEffectSystem::Update. base_color_1/base_color_2 are the struck
// entity's own RenderableComponent colors from just before the flash
// started, restored once elapsed reaches the flash's total duration.
// Deliberately NOT meta-registered -- never stamped via prefab, never
// cloned/described -- this is purely engine-internal per-frame state, not
// content data, same reasoning as TweenComponent.
struct DamageFlashComponent
{
    Color base_color_1;
    Color base_color_2;
    float elapsed = 0.0f;
};

} // namespace psr
