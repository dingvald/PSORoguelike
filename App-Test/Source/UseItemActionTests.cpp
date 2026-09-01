#include "Actions/UseItemAction.h"

#include "CombatRegistrySetup.h"
#include "Components/ConsumableComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/TPComponent.h"
#include "Engine/Combat/HealEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Items/ItemUseEvent.h"
#include "Engine/World/Grid.h"
#include "Items/AffixLibrary.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace {

// Tag type identifying this test file's subscriptions to EventHandlerComponent
// -- never instantiated, only used as Subscribe/Unsubscribe's TOwner key.
struct ItemUseProbe
{
};

entt::entity MakeConsumable(psr::Registry& registry, psr::ConsumableEffect effect, int amount, std::uint32_t prefab_id)
{
    entt::entity item = registry.CreateEntity();
    registry.Emplace<psr::ConsumableComponent>(item, psr::ConsumableComponent{effect, amount});
    registry.Emplace<psr::PrefabIdComponent>(item, psr::PrefabIdComponent{prefab_id});
    return item;
}

} // namespace

TEST_CASE("UseItemAction is a free no-op when the actor has no InventoryComponent", "[UseItemAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);

    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});

    psr::UseItemAction action(/*inventory_index=*/0);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("UseItemAction is a free no-op for an out-of-range index", "[UseItemAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);

    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    actor.Emplace<psr::InventoryComponent>();

    psr::UseItemAction action(/*inventory_index=*/0);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
}

TEST_CASE("UseItemAction is a free no-op for an item with no ConsumableComponent", "[UseItemAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);

    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});

    entt::entity item = registry.CreateEntity(); // no ConsumableComponent -- e.g. a weapon
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{item}, 20});

    psr::UseItemAction action(/*inventory_index=*/0);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == 0);
    REQUIRE(actor.Get<psr::InventoryComponent>().items == std::vector<entt::entity>{item});
    REQUIRE(registry.IsValid(item));
}

TEST_CASE("UseItemAction heals HP clamped to max_hp, consumes the item, and dispatches AfterItemUseEvent",
          "[UseItemAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);

    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    actor.Emplace<psr::HealthComponent>(psr::HealthComponent{90, 100});

    entt::entity item = MakeConsumable(registry, psr::ConsumableEffect::RestoreHp, /*amount=*/30, /*prefab_id=*/11);
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{item}, 20});

    std::optional<psr::AfterItemUseEvent> received;
    actor.Get<psr::EventHandlerComponent>().Subscribe<psr::AfterItemUseEvent, ItemUseProbe>(
        [&](psr::Entity, psr::AfterItemUseEvent& event) { received = event; });
    std::optional<psr::AfterHealEvent> received_heal;
    actor.Get<psr::EventHandlerComponent>().Subscribe<psr::AfterHealEvent, ItemUseProbe>(
        [&](psr::Entity, psr::AfterHealEvent& event) { received_heal = event; });

    psr::UseItemAction action(/*inventory_index=*/0);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::UseItemAction::kUseItemCost);
    CHECK(actor.Get<psr::HealthComponent>().current_hp == 100); // clamped, not 120
    CHECK(actor.Get<psr::InventoryComponent>().items.empty());
    CHECK_FALSE(registry.IsValid(item));
    REQUIRE(received.has_value());
    CHECK(received->item_prefab_id == 11);
    REQUIRE(received_heal.has_value());
    CHECK(received_heal->amount == 10); // actually applied (100 - 90), not the requested 30
}

TEST_CASE("UseItemAction restores TP clamped to max_tp and consumes the item", "[UseItemAction]")
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);

    entt::entity handle = registry.CreateEntity();
    psr::Entity actor(registry, handle);
    actor.Emplace<psr::Position>(psr::Vec2{1, 1});
    actor.Emplace<psr::TPComponent>(psr::TPComponent{10, 50});

    entt::entity item = MakeConsumable(registry, psr::ConsumableEffect::RestoreTp, /*amount=*/100, /*prefab_id=*/22);
    actor.Emplace<psr::InventoryComponent>(psr::InventoryComponent{{item}, 20});

    psr::UseItemAction action(/*inventory_index=*/0);
    psr::ActionResult result = action.Perform(actor);

    REQUIRE(result.cost == psr::UseItemAction::kUseItemCost);
    CHECK(actor.Get<psr::TPComponent>().current_tp == 50); // clamped, not 110
    CHECK(actor.Get<psr::InventoryComponent>().items.empty());
    CHECK_FALSE(registry.IsValid(item));
}
