#include "Hub/HubInteraction.h"

#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

namespace psr {

std::optional<InteractionType> FindInteractableAt(const Registry& registry, const Grid& grid, Vec2 tile)
{
    for (entt::entity entity : grid.GetEntities(tile))
    {
        if (const InteractableComponent* interactable = registry.TryGetComponent<InteractableComponent>(entity))
            return interactable->interaction_type;
    }
    return std::nullopt;
}

} // namespace psr
