#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <cstdint>

namespace psr {

// Which VFX prefab (if any) to spawn at the target's tile when whatever
// carries this component lands a hit -- authored on weapon prefabs today
// (covers both plain attacks and Photon Arts, which are channeled through
// the wielded weapon), read via EquipmentComponent's own event-fill seam
// (see ContributeAttack/ContributePhotonArtCast in EquipmentComponent.cpp)
// and threaded through IncomingDamageEvent/AfterDamageEvent to
// OnHitEffectSystem, which does the actual spawning. Techniques don't use
// this component -- they aren't ECS entities -- and instead carry the same
// two values as plain fields on Technique itself.
struct OnHitEffectComponent
{
    std::uint32_t effect_prefab_id = 0;
    float duration = 0.3f;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<OnHitEffectComponent>("on_hit_effect")
            .Data<&OnHitEffectComponent::effect_prefab_id>("effect_prefab_id")
            .Data<&OnHitEffectComponent::duration>("duration");
    }
};

} // namespace psr
