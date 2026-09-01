#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <cstdint>

namespace psr {

// Which DropTable an enemy/boss prefab rolls loot from on death, a NameId
// into DropTableLibrary -- same shape/convention as RaceComponent::race_id.
// Enemies with no DropTableComponent simply drop nothing (LootDropSystem
// no-ops).
struct DropTableComponent
{
    std::uint32_t drop_table_id = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<DropTableComponent>("drop_table").Data<&DropTableComponent::drop_table_id>("drop_table_id");
    }
};

} // namespace psr
