#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <cstdint>
#include <entt/entt.hpp>

namespace psr {

// Which weapon prefab a spawned enemy should be equipped with, since there's
// no inventory/equip system to do this interactively -- GameplayLayer's
// post-spawn hook reads this once (weapon_prefab_id, a NameId into Entities/,
// same convention as RaceComponent::race_id) to create the weapon entity and
// set EquipmentComponent::weapon. Kept on the entity afterward purely so
// AttachHandlers below can find and destroy that weapon entity when its
// wielder dies -- an innate weapon isn't lootable, so nothing else will ever
// clean it up.
struct InnateWeaponComponent
{
    std::uint32_t weapon_prefab_id = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<InnateWeaponComponent>("innate_weapon")
            .Data<&InnateWeaponComponent::weapon_prefab_id>("weapon_prefab_id");
    }

    static void AttachHandlers(entt::registry& registry, entt::entity entity);
    static void DetachHandlers(entt::registry& registry, entt::entity entity);
};

} // namespace psr
