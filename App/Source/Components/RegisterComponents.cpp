#include "Components/RegisterComponents.h"

#include "Components/AiComponent.h"
#include "Components/BlocksMovementComponent.h"
#include "Components/CurrencyComponent.h"
#include "Components/DropTableComponent.h"
#include "Components/EnergyComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/HotbarComponent.h"
#include "Components/InnateWeaponComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RaceComponent.h"
#include "Components/RenderableComponent.h"
#include "Components/SectionIdComponent.h"
#include "Components/StatsComponent.h"
#include "Components/StatusEffectComponent.h"
#include "Components/TPComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/Combat/DeathSystem.h"
#include "Engine/Combat/HealthSystem.h"
#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/ModComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/RarityComponent.h"

namespace psr {

EntitySchemaModel RegisterComponents(Registry& registry)
{
    ComponentSchemaRegistrar reg{registry.GetMetaContext()};

    AiComponent::Register(reg);
    ArmorComponent::Register(reg);
    BlocksMovementComponent::Register(reg);
    CurrencyComponent::Register(reg);
    DropTableComponent::Register(reg);
    EnergyComponent::Register(reg);
    HealthComponent::Register(reg);
    HotbarComponent::Register(reg);
    InnateWeaponComponent::Register(reg);
    ModComponent::Register(reg);
    PlayerControlledComponent::Register(reg);
    PrefabIdComponent::Register(reg);
    Position::Register(reg);
    RaceComponent::Register(reg);
    RarityComponent::Register(reg);
    RenderableComponent::Register(reg);
    SectionIdComponent::Register(reg);
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
    // so this is safe to call regardless. InnateWeaponComponent is meta-
    // registered above (it's authorable) but still needs this call for its
    // own DeathEvent-handler wiring, same mechanism, unrelated reason.
    registry.BindComponentEvents<EquipmentComponent>();
    registry.BindComponentEvents<TPComponent>();
    registry.BindComponentEvents<StatusEffectComponent>();
    registry.BindComponentEvents<InnateWeaponComponent>();

    // HealthSystem/DeathSystem react to HealthComponent's own lifecycle
    // rather than being HealthComponent's own AttachHandlers -- see
    // HealthSystem.h's doc comment -- so they're wired via BindSystemEvents
    // instead of the BindComponentEvents calls above.
    registry.BindSystemEvents<HealthComponent, HealthSystem>();
    registry.BindSystemEvents<HealthComponent, DeathSystem>();

    return reg.Model();
}

} // namespace psr
