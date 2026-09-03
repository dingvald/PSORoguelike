#include "Items/Hotbar.h"

#include "Components/ConsumableComponent.h"
#include "Components/HotbarComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("AssignItemToHotbarSlot binds the item's prefab id into the target Item slot", "[Hotbar]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity potion = registry.CreateEntity();
    registry.Emplace<psr::ConsumableComponent>(potion);
    registry.Emplace<psr::PrefabIdComponent>(potion, psr::PrefabIdComponent{42});
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{potion}, 20});

    REQUIRE(psr::AssignItemToHotbarSlot(actor, 0, 3));

    const psr::HotbarSlot& slot = actor.Get<psr::HotbarComponent>().slots[3];
    REQUIRE(slot.type == psr::HotbarSlotType::Item);
    REQUIRE(slot.id == 42);
}

TEST_CASE("AssignItemToHotbarSlot overwrites whatever previously occupied the slot", "[Hotbar]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity potion = registry.CreateEntity();
    registry.Emplace<psr::ConsumableComponent>(potion);
    registry.Emplace<psr::PrefabIdComponent>(potion, psr::PrefabIdComponent{7});
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{potion}, 20});

    psr::HotbarComponent hotbar;
    hotbar.slots[0] = psr::HotbarSlot{psr::HotbarSlotType::Technique, 99};
    actor.Emplace<psr::HotbarComponent>(hotbar);

    REQUIRE(psr::AssignItemToHotbarSlot(actor, 0, 0));

    const psr::HotbarSlot& slot = actor.Get<psr::HotbarComponent>().slots[0];
    REQUIRE(slot.type == psr::HotbarSlotType::Item);
    REQUIRE(slot.id == 7);
}

TEST_CASE("AssignItemToHotbarSlot is a no-op for a non-consumable item", "[Hotbar]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity weapon = registry.CreateEntity();
    registry.Emplace<psr::WeaponComponent>(weapon);
    registry.Emplace<psr::PrefabIdComponent>(weapon, psr::PrefabIdComponent{1});
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{weapon}, 20});

    REQUIRE_FALSE(psr::AssignItemToHotbarSlot(actor, 0, 0));
    REQUIRE_FALSE(actor.Has<psr::HotbarComponent>());
}

TEST_CASE("AssignItemToHotbarSlot is a no-op for an out-of-range inventory_index", "[Hotbar]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::InventoryComponent>();

    REQUIRE_FALSE(psr::AssignItemToHotbarSlot(actor, 0, 0));
}

TEST_CASE("AssignItemToHotbarSlot is a no-op for an out-of-range hotbar_slot", "[Hotbar]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity potion = registry.CreateEntity();
    registry.Emplace<psr::ConsumableComponent>(potion);
    registry.Emplace<psr::PrefabIdComponent>(potion, psr::PrefabIdComponent{42});
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{potion}, 20});

    REQUIRE_FALSE(psr::AssignItemToHotbarSlot(actor, 0, -1));
    REQUIRE_FALSE(psr::AssignItemToHotbarSlot(actor, 0, static_cast<int>(psr::HotbarComponent::kSlotCount)));
}

TEST_CASE("AssignItemToHotbarSlot is a no-op for a consumable missing PrefabIdComponent", "[Hotbar]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);

    entt::entity potion = registry.CreateEntity();
    registry.Emplace<psr::ConsumableComponent>(potion);
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{potion}, 20});

    REQUIRE_FALSE(psr::AssignItemToHotbarSlot(actor, 0, 0));
    REQUIRE_FALSE(actor.Has<psr::HotbarComponent>());
}
