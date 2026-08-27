#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// Current/max Technique Points -- the single resource pool both Technique
// (see TechniqueAction) and PhotonArt (see PhotonArtAction) spend. PP and TP
// were originally separate pools (Hunter/Ranger PP vs. Force TP) but were
// collapsed into one per docs/GDD.md's "PP vs. TP (revised -- collapsed to one pool)" section --
// same shape as HealthComponent: no regen mechanic this round, no fixed
// class-to-pool enforcement.
struct TPComponent
{
    int current_tp = 0;
    int max_tp = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<TPComponent>("tp")
            .Data<&TPComponent::current_tp>("current_tp")
            .Data<&TPComponent::max_tp>("max_tp");
    }
};

} // namespace psr
