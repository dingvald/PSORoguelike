#include "Combat/DisplayName.h"

#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/PrefabIdComponent.h"

#include <optional>

namespace psr {

std::string DisplayName(Registry& registry, entt::entity entity, entt::entity player)
{
    if (entity == player)
        return "Player";

    if (const PrefabIdComponent* prefab_id = registry.TryGetComponent<PrefabIdComponent>(entity))
    {
        if (std::optional<std::string> label = NameIdRegistry::Find(prefab_id->value))
            return *label;
    }
    return "something";
}

} // namespace psr
