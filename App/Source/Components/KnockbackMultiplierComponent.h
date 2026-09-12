#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// Scales the 1-tile push WeaponAttackAction.cpp's ApplyKnockback applies on a
// landed melee hit -- a multiplier of 0 (player.json, box_metal.json,
// box_wood.json) suppresses the push entirely; anything without this
// component defaults to 1 (normal knockback). Not yet a continuous scale
// (the push is a fixed 1-tile step, not distance * multiplier) -- today it's
// just a 0-vs-nonzero gate.
struct KnockbackMultiplierComponent
{
    float multiplier = 1.0f;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<KnockbackMultiplierComponent>("knockback_multiplier")
            .Data<&KnockbackMultiplierComponent::multiplier>("multiplier");
    }
};

} // namespace psr
