#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <cstdint>

namespace psr {

// The prefab_id a runtime entity was cloned from -- stamped automatically by
// Registry::CreateEntity(prefab_id), never by hand. Lets code that only has
// an entity handle (e.g. an inventory bucketing items by "same kind") ask
// "what prefab made this" without threading the id through separately.
// Prefab template entities themselves never carry one -- only their runtime
// clones do.
struct PrefabIdComponent
{
    std::uint32_t value = 0;

    // Not authorable -- this is a derived-at-instantiation fact, not
    // something content should set. Still fully clone-eligible --
    // RegisterComponent<T> binds that independent of authorable/.Data<>().
    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<PrefabIdComponent>("prefab_id", /*authorable=*/false);
    }
};

} // namespace psr
