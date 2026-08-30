#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// The six PSO-derived core stats (see docs/GDD.md): ATP (attack power), ATA
// (accuracy), MST (technique/magic power), DFP (defense), EVP (evasion), LCK
// (luck -- biases rare triggers). No default values beyond zero are implied
// here -- real per-entity numbers are authored content (Prefab Editor /
// M5.2's entity editor), not engine-decided.
struct StatsComponent
{
    int atp = 0;
    int ata = 0;
    int mst = 0;
    int dfp = 0;
    int evp = 0;
    int lck = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<StatsComponent>("stats")
            .Data<&StatsComponent::atp>("atp")
            .Data<&StatsComponent::ata>("ata")
            .Data<&StatsComponent::mst>("mst")
            .Data<&StatsComponent::dfp>("dfp")
            .Data<&StatsComponent::evp>("evp")
            .Data<&StatsComponent::lck>("lck");
    }
};

} // namespace psr
