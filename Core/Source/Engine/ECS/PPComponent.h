#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// Current/max Photon Points -- the resource Hunter/Ranger Photon Arts spend
// (see PhotonArtAction). Same shape as HealthComponent: theme-agnostic, no
// default values beyond zero implied here, no regen mechanic (docs/GDD.md
// says nothing about PP regen); real per-entity numbers are authored content.
// Not tied to any fixed class -- a prefab simply doesn't carry this component
// if it never spends PP, same philosophy as RaceComponent's no-fixed-enum
// approach.
struct PPComponent
{
    int current_pp = 0;
    int max_pp = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<PPComponent>("pp")
            .Data<&PPComponent::current_pp>("current_pp")
            .Data<&PPComponent::max_pp>("max_pp");
    }
};

} // namespace psr
