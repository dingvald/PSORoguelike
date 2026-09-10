#include "Items/Shop.h"

#include "Components/CurrencyComponent.h"
#include "Components/InventoryComponent.h"
#include "Engine/ECS/ComponentMeta.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/IEntityLoader.h"
#include "Engine/ECS/ItemComponent.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/ValueComponent.h"
#include "Shop/ShopStock.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/core/hashed_string.hpp>

#include <unordered_map>
#include <vector>

namespace {

using namespace psr;

// Throwaway test fixture prefab -- not real content, per CLAUDE.md's
// test-fixture carve-out, same pattern LootDropSystemTests.cpp's own
// ItemMarker/TestEntityLoader already established.
struct ItemMarker
{
    static void Register(entt::meta_ctx& ctx)
    {
        using namespace entt::literals;
        entt::meta_factory<ItemMarker>(ctx).func<&psr::CloneComponent<ItemMarker>>("clone"_hs);
    }
};

const std::uint32_t kTestItemId = entt::hashed_string::value("test_item");
const std::uint32_t kStackableTestItemId = entt::hashed_string::value("stackable_test_item");

class TestEntityLoader : public IEntityLoader
{
public:
    bool Load(std::filesystem::path /*path*/) override { return true; }

    void Populate(entt::registry& prefab_registry,
                  std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
    {
        entt::entity item = prefab_registry.create();
        prefab_registry.emplace<ItemMarker>(item);
        out_prefab_ids.emplace(kTestItemId, item);

        entt::entity stackable_item = prefab_registry.create();
        prefab_registry.emplace<ItemComponent>(stackable_item, ItemComponent{/*max_stack=*/5, /*quantity=*/1});
        out_prefab_ids.emplace(kStackableTestItemId, stackable_item);
    }
};

// Registers ItemMarker (a manual clone func, same as the existing precedent)
// and ItemComponent (via the real ComponentSchemaRegistrar, since BuyItem's
// stacking merge needs Registry::CreateEntity(prefab_id) to actually clone
// the stackable prefab's ItemComponent value onto each purchased instance).
void RegisterTestPrefabTypes(Registry& registry)
{
    ItemMarker::Register(registry.GetMetaContext());
    ComponentSchemaRegistrar reg{registry.GetMetaContext()};
    ItemComponent::Register(reg);
}

} // namespace

TEST_CASE("BuyItem succeeds, debits Meseta, and creates an inventory entry", "[Shop]")
{
    Registry registry;
    RegisterTestPrefabTypes(registry);
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    entt::entity handle = registry.CreateEntity();
    Entity actor(registry, handle);
    actor.Emplace<CurrencyComponent>(CurrencyComponent{100});
    actor.Emplace<InventoryComponent>();

    ShopStock stock;
    stock.entries.push_back(ShopStockEntry{"test_item", 30});

    REQUIRE(BuyItem(actor, stock, 0));

    REQUIRE(actor.Get<CurrencyComponent>().meseta == 70);
    REQUIRE(actor.Get<InventoryComponent>().items.size() == 1);
}

TEST_CASE("BuyItem is a no-op when unaffordable", "[Shop]")
{
    Registry registry;
    RegisterTestPrefabTypes(registry);
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    entt::entity handle = registry.CreateEntity();
    Entity actor(registry, handle);
    actor.Emplace<CurrencyComponent>(CurrencyComponent{10});
    actor.Emplace<InventoryComponent>();

    ShopStock stock;
    stock.entries.push_back(ShopStockEntry{"test_item", 30});

    REQUIRE_FALSE(BuyItem(actor, stock, 0));
    REQUIRE(actor.Get<CurrencyComponent>().meseta == 10);
    REQUIRE(actor.Get<InventoryComponent>().items.empty());
}

TEST_CASE("BuyItem is a no-op when the inventory is already at capacity", "[Shop]")
{
    Registry registry;
    RegisterTestPrefabTypes(registry);
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    entt::entity handle = registry.CreateEntity();
    Entity actor(registry, handle);
    actor.Emplace<CurrencyComponent>(CurrencyComponent{100});
    actor.Emplace<InventoryComponent>(InventoryComponent{{}, 0});

    ShopStock stock;
    stock.entries.push_back(ShopStockEntry{"test_item", 30});

    REQUIRE_FALSE(BuyItem(actor, stock, 0));
    REQUIRE(actor.Get<CurrencyComponent>().meseta == 100);
}

TEST_CASE("BuyItem is a no-op for an out-of-range stock index", "[Shop]")
{
    Registry registry;
    entt::entity handle = registry.CreateEntity();
    Entity actor(registry, handle);
    actor.Emplace<CurrencyComponent>(CurrencyComponent{100});
    actor.Emplace<InventoryComponent>();

    ShopStock stock;
    REQUIRE_FALSE(BuyItem(actor, stock, 0));
}

TEST_CASE("BuyItem is a no-op for an unresolvable prefab id", "[Shop]")
{
    Registry registry;
    entt::entity handle = registry.CreateEntity();
    Entity actor(registry, handle);
    actor.Emplace<CurrencyComponent>(CurrencyComponent{100});
    actor.Emplace<InventoryComponent>();

    ShopStock stock;
    stock.entries.push_back(ShopStockEntry{"nonexistent", 10});

    REQUIRE_FALSE(BuyItem(actor, stock, 0));
}

TEST_CASE("BuyItem merges a stackable purchase into an existing matching slot instead of opening a new one",
          "[Shop]")
{
    Registry registry;
    RegisterTestPrefabTypes(registry);
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    entt::entity handle = registry.CreateEntity();
    Entity actor(registry, handle);
    actor.Emplace<CurrencyComponent>(CurrencyComponent{100});
    actor.Emplace<InventoryComponent>();

    ShopStock stock;
    stock.entries.push_back(ShopStockEntry{"stackable_test_item", 10});

    REQUIRE(BuyItem(actor, stock, 0));
    REQUIRE(BuyItem(actor, stock, 0));
    REQUIRE(BuyItem(actor, stock, 0));

    REQUIRE(actor.Get<CurrencyComponent>().meseta == 70);
    REQUIRE(actor.Get<InventoryComponent>().items.size() == 1);
    const entt::entity item = actor.Get<InventoryComponent>().items[0];
    CHECK(registry.GetComponent<ItemComponent>(item).quantity == 3);
}

TEST_CASE("BuyItem is a no-op once a stackable purchase's matching slot is already at max_stack", "[Shop]")
{
    Registry registry;
    RegisterTestPrefabTypes(registry);
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    entt::entity handle = registry.CreateEntity();
    Entity actor(registry, handle);
    actor.Emplace<CurrencyComponent>(CurrencyComponent{100});

    entt::entity held = registry.CreateEntity(kStackableTestItemId);
    registry.GetComponent<ItemComponent>(held).quantity = 5; // already at max_stack
    actor.Emplace<InventoryComponent>(InventoryComponent{{held}, 20});

    ShopStock stock;
    stock.entries.push_back(ShopStockEntry{"stackable_test_item", 10});

    REQUIRE_FALSE(BuyItem(actor, stock, 0));
    REQUIRE(actor.Get<CurrencyComponent>().meseta == 100);
    REQUIRE(actor.Get<InventoryComponent>().items == std::vector<entt::entity>{held});
    CHECK(registry.GetComponent<ItemComponent>(held).quantity == 5);
}

TEST_CASE("SellItem succeeds, credits Meseta by ValueComponent, and destroys the item", "[Shop]")
{
    Registry registry;
    entt::entity handle = registry.CreateEntity();
    Entity actor(registry, handle);

    entt::entity item = registry.CreateEntity();
    registry.Emplace<ValueComponent>(item, ValueComponent{25});
    actor.Emplace<InventoryComponent>(InventoryComponent{{item}, 20});
    actor.Emplace<CurrencyComponent>();

    REQUIRE(SellItem(actor, 0));

    REQUIRE(actor.Get<CurrencyComponent>().meseta == 25);
    REQUIRE(actor.Get<InventoryComponent>().items.empty());
    REQUIRE_FALSE(registry.IsValid(item));
}

TEST_CASE("SellItem sells for 0 when the item carries no ValueComponent", "[Shop]")
{
    Registry registry;
    entt::entity handle = registry.CreateEntity();
    Entity actor(registry, handle);

    entt::entity item = registry.CreateEntity();
    actor.Emplace<InventoryComponent>(InventoryComponent{{item}, 20});
    actor.Emplace<CurrencyComponent>();

    REQUIRE(SellItem(actor, 0));
    REQUIRE(actor.Get<CurrencyComponent>().meseta == 0);
    REQUIRE(actor.Get<InventoryComponent>().items.empty());
}

TEST_CASE("SellItem is a no-op for an out-of-range index", "[Shop]")
{
    Registry registry;
    entt::entity handle = registry.CreateEntity();
    Entity actor(registry, handle);
    actor.Emplace<InventoryComponent>();

    REQUIRE_FALSE(SellItem(actor, 0));
}
