#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <cstdint>

namespace psr {

// Which of the four enemy races (docs/GDD.md: Native/A.Beast/Machine/Dark)
// this prefab belongs to -- consumed later by M7.1's four-race weapon damage
// bonus. Authored as a NameId (a string hashed via entt::hashed_string, same
// convention as RenderableComponent::texture_id) rather than a compile-time
// enum, so new races can be added or removed purely as data, with no engine
// recompile.
struct RaceComponent
{
    std::uint32_t race_id = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<RaceComponent>("race").Data<&RaceComponent::race_id>("race_id");
    }
};

} // namespace psr
