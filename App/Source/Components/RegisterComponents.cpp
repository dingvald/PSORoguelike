#include "Components/RegisterComponents.h"

#include "Components/RenderableComponent.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/SocketComponent.h"

namespace psr {

EntitySchemaModel RegisterComponents(Registry& registry)
{
    ComponentSchemaRegistrar reg{registry.GetMetaContext()};

    PrefabIdComponent::Register(reg);
    Position::Register(reg);
    RenderableComponent::Register(reg);
    SocketComponent::Register(reg);

    return reg.Model();
}

} // namespace psr
