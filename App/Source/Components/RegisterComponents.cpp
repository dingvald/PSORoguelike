#include "Components/RegisterComponents.h"

#include "Components/BlocksMovementComponent.h"
#include "Components/EnergyComponent.h"
#include "Components/GroundItemComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RenderableComponent.h"
#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/DropTableComponent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/MaterialComponent.h"
#include "Engine/ECS/MesetaComponent.h"
#include "Engine/ECS/ModComponent.h"
#include "Engine/ECS/PPComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/RaceComponent.h"
#include "Engine/ECS/RareVariantComponent.h"
#include "Engine/ECS/RarityComponent.h"
#include "Engine/ECS/SectionIdComponent.h"
#include "Engine/ECS/SocketComponent.h"
#include "Engine/ECS/StatsComponent.h"
#include "Engine/ECS/TPComponent.h"
#include "Engine/ECS/WeaponComponent.h"

namespace psr {

EntitySchemaModel RegisterComponents(Registry& registry)
{
    ComponentSchemaRegistrar reg{registry.GetMetaContext()};

    ArmorComponent::Register(reg);
    BlocksMovementComponent::Register(reg);
    DropTableComponent::Register(reg);
    EnergyComponent::Register(reg);
    GroundItemComponent::Register(reg);
    HealthComponent::Register(reg);
    MaterialComponent::Register(reg);
    MesetaComponent::Register(reg);
    ModComponent::Register(reg);
    PlayerControlledComponent::Register(reg);
    PPComponent::Register(reg);
    PrefabIdComponent::Register(reg);
    Position::Register(reg);
    RaceComponent::Register(reg);
    RareVariantComponent::Register(reg);
    RarityComponent::Register(reg);
    RenderableComponent::Register(reg);
    SectionIdComponent::Register(reg);
    SocketComponent::Register(reg);
    StatsComponent::Register(reg);
    TPComponent::Register(reg);
    WeaponComponent::Register(reg);

    return reg.Model();
}

} // namespace psr
