#include "Components/RegisterComponents.h"

#include "Components/RenderableComponent.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/RaceComponent.h"
#include "Engine/ECS/SocketComponent.h"
#include "Engine/ECS/StatsComponent.h"

namespace psr {

EntitySchemaModel RegisterComponents(Registry& registry)
{
    ComponentSchemaRegistrar reg{registry.GetMetaContext()};

    PrefabIdComponent::Register(reg);
    Position::Register(reg);
    RaceComponent::Register(reg);
    RenderableComponent::Register(reg);
    SocketComponent::Register(reg);
    StatsComponent::Register(reg);

    return reg.Model();
}

} // namespace psr
