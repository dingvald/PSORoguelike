#include "Components/InnateWeaponComponent.h"

#include "Components/EquipmentComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/Combat/DeathSystem.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("InnateWeaponComponent registers as authorable with a single NameId field", "[EntitySchema]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::InnateWeaponComponent::Register(reg);
    const psr::EntitySchemaModel model = reg.Model();

    REQUIRE(model.components.size() == 1);
    const psr::ComponentSchema& innate_weapon = model.components[0];
    CHECK(innate_weapon.id == "innate_weapon");
    CHECK(innate_weapon.authorable);
    REQUIRE(innate_weapon.fields.size() == 1);
    CHECK(innate_weapon.fields[0].name == "weapon_prefab_id");
    CHECK(innate_weapon.fields[0].kind == psr::FieldKind::NameId);
}

TEST_CASE("InnateWeaponComponent destroys its equipped weapon before DeathSystem destroys the wielder",
          "[InnateWeaponComponent]")
{
    psr::Registry registry;
    registry.BindComponentEvents<psr::InnateWeaponComponent>();
    registry.BindSystemEvents<psr::HealthComponent, psr::DeathSystem>();

    const entt::entity weapon = registry.CreateEntity();

    const entt::entity wielder = registry.CreateEntity();
    registry.Emplace<psr::InnateWeaponComponent>(wielder, psr::InnateWeaponComponent{0});
    registry.Emplace<psr::EquipmentComponent>(wielder, psr::EquipmentComponent{weapon});
    registry.Emplace<psr::HealthComponent>(wielder, psr::HealthComponent{0, 10});

    psr::DeathEvent death_event;
    psr::Entity(registry, wielder).Dispatch(death_event);

    CHECK_FALSE(registry.IsValid(weapon));
    CHECK_FALSE(registry.IsValid(wielder));
}

TEST_CASE("InnateWeaponComponent is a no-op on death when no weapon was ever equipped", "[InnateWeaponComponent]")
{
    psr::Registry registry;
    registry.BindComponentEvents<psr::InnateWeaponComponent>();
    registry.BindSystemEvents<psr::HealthComponent, psr::DeathSystem>();

    const entt::entity wielder = registry.CreateEntity();
    registry.Emplace<psr::InnateWeaponComponent>(wielder, psr::InnateWeaponComponent{0});
    registry.Emplace<psr::HealthComponent>(wielder, psr::HealthComponent{0, 10});

    psr::DeathEvent death_event;
    psr::Entity(registry, wielder).Dispatch(death_event);

    CHECK_FALSE(registry.IsValid(wielder));
}
