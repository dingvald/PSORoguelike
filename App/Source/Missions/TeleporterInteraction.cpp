#include "Missions/TeleporterInteraction.h"

#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

namespace psr {

std::optional<TeleporterDestination> FindTeleporterAt(const Registry& registry, const Grid& grid, Vec2 tile)
{
    for (entt::entity entity : grid.GetEntities(tile))
    {
        if (const TeleporterComponent* teleporter = registry.TryGetComponent<TeleporterComponent>(entity))
            return teleporter->destination;
    }
    return std::nullopt;
}

} // namespace psr
