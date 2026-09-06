#include "Items/Shop.h"

#include "Components/CurrencyComponent.h"
#include "Components/InventoryComponent.h"
#include "Engine/ECS/ComponentMeta.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/IEntityLoader.h"
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
    }
};

void RegisterTestPrefabTypes(Registry& registry)
{
    ItemMarker::Register(registry.GetMetaContext());
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
