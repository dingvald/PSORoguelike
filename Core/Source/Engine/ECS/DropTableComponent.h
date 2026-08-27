#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <cstdint>

namespace psr {

// Which DropTable (Engine/Items/DropTable.h) this prefab rolls against on
// death -- a NameId into DropTableLibrary, same convention as
// RaceComponent::race_id/WeaponComponent::prefix_affix_id. 0 = no loot.
// Consumed by LootDropSystem (App/Source/Systems/LootDropSystem.h) via
// Registry::OnDestroy<HealthComponent>.
struct DropTableComponent
{
    std::uint32_t drop_table_id = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<DropTableComponent>("loot").Data<&DropTableComponent::drop_table_id>("drop_table_id");
    }
};

} // namespace psr
