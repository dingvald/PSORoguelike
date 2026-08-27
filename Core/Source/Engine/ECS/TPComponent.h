#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// Current/max Technique Points -- the resource Force Techniques spend (see
// TechniqueAction). Same shape/deferral as PPComponent: no regen mechanic
// this round, no fixed class-to-pool enforcement.
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
