#include "Components/RegisterComponents.h"

#include "Components/BlocksMovementComponent.h"
#include "Components/EnergyComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RenderableComponent.h"
#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/ModComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/RaceComponent.h"
#include "Engine/ECS/RarityComponent.h"
#include "Engine/ECS/SocketComponent.h"
#include "Engine/ECS/StatsComponent.h"
#include "Engine/ECS/WeaponComponent.h"

namespace psr {

EntitySchemaModel RegisterComponents(Registry& registry)
{
    ComponentSchemaRegistrar reg{registry.GetMetaContext()};

    ArmorComponent::Register(reg);
    BlocksMovementComponent::Register(reg);
    EnergyComponent::Register(reg);
    ModComponent::Register(reg);
    PlayerControlledComponent::Register(reg);
    PrefabIdComponent::Register(reg);
    Position::Register(reg);
    RaceComponent::Register(reg);
    RarityComponent::Register(reg);
    RenderableComponent::Register(reg);
    SocketComponent::Register(reg);
    StatsComponent::Register(reg);
    WeaponComponent::Register(reg);

    return reg.Model();
}

} // namespace psr
