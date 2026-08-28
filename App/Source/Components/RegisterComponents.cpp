#include "Components/RegisterComponents.h"

#include "Components/BlocksMovementComponent.h"
#include "Components/EnergyComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/HotbarComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RenderableComponent.h"
#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/ModComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/RaceComponent.h"
#include "Engine/ECS/RarityComponent.h"
#include "Engine/ECS/SocketComponent.h"
#include "Engine/ECS/StatsComponent.h"
#include "Engine/ECS/StatusEffectComponent.h"
#include "Engine/ECS/TPComponent.h"
#include "Engine/ECS/WeaponComponent.h"

namespace psr {

EntitySchemaModel RegisterComponents(Registry& registry)
{
    ComponentSchemaRegistrar reg{registry.GetMetaContext()};

    ArmorComponent::Register(reg);
    BlocksMovementComponent::Register(reg);
    EnergyComponent::Register(reg);
    HealthComponent::Register(reg);
    HotbarComponent::Register(reg);
    ModComponent::Register(reg);
    PlayerControlledComponent::Register(reg);
    PrefabIdComponent::Register(reg);
    Position::Register(reg);
    RaceComponent::Register(reg);
    RarityComponent::Register(reg);
    RenderableComponent::Register(reg);
    SocketComponent::Register(reg);
    StatsComponent::Register(reg);
    TPComponent::Register(reg);
    WeaponComponent::Register(reg);

    // EquipmentComponent/StatusEffectComponent are deliberately not
    // meta/schema-registered above (entt::entity has no FieldKind;
    // StatusEffectComponent is runtime-only accumulated state, never
    // hand-authored in a prefab -- see its own doc comment), but they and
    // TPComponent still need their own event handlers wired -- see each
    // component's own AttachHandlers/DetachHandlers doc comment.
    // Registry::BindComponentEvents just connects entt's
    // on_construct/on_destroy<T> signals, independent of meta registration,
    // so this is safe to call regardless.
    registry.BindComponentEvents<EquipmentComponent>();
    registry.BindComponentEvents<TPComponent>();
    registry.BindComponentEvents<StatusEffectComponent>();

    return reg.Model();
}

} // namespace psr
