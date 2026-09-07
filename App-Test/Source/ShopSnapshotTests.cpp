#include "Shop/ShopSnapshot.h"

#include "Components/CurrencyComponent.h"
#include "Components/InventoryComponent.h"
#include "Engine/ECS/ComponentMeta.h"
#include "Engine/ECS/IEntityLoader.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/ValueComponent.h"
#include "Items/AffixLibrary.h"
#include "Shop/ShopStock.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/core/hashed_string.hpp>

#include <unordered_map>

namespace {

using namespace psr;

psr::AffixLibrary g_no_affixes;

// Throwaway test fixture prefab -- not real content, per CLAUDE.md's
// test-fixture carve-out, same pattern LootDropSystemTests.cpp/ShopTests.cpp
// already establish.
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

} // namespace

TEST_CASE("BuildShopMessage flags a stock entry affordable/unaffordable against current Meseta",
          "[ShopSnapshot]")
{
    Registry registry;
    ItemMarker::Register(registry.GetMetaContext());
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    entt::entity player = registry.CreateEntity();
    registry.Emplace<CurrencyComponent>(player, CurrencyComponent{50});
    registry.Emplace<InventoryComponent>(player);

    ShopStock stock;
    stock.entries.push_back(ShopStockEntry{"test_item", 30});
    stock.entries.push_back(ShopStockEntry{"test_item", 100});

    const ShopMessage message = BuildShopMessage(registry, player, stock, g_no_affixes);

    REQUIRE(message.current_meseta == 50);
    REQUIRE(message.stock.size() == 2);
    REQUIRE(message.stock[0].affordable);
    REQUIRE_FALSE(message.stock[1].affordable);
}

TEST_CASE("BuildShopMessage falls back to the raw prefab id string for an unresolvable stock entry",
          "[ShopSnapshot]")
{
    Registry registry;
    entt::entity player = registry.CreateEntity();
    registry.Emplace<CurrencyComponent>(player);
    registry.Emplace<InventoryComponent>(player);

    ShopStock stock;
    stock.entries.push_back(ShopStockEntry{"nonexistent", 10});

    const ShopMessage message = BuildShopMessage(registry, player, stock, g_no_affixes);

    REQUIRE(message.stock.size() == 1);
    REQUIRE(message.stock[0].display_name == "nonexistent");
}

TEST_CASE("BuildShopMessage lists the player's inventory as sellable entries with their ValueComponent",
          "[ShopSnapshot]")
{
    Registry registry;
    entt::entity player = registry.CreateEntity();
    registry.Emplace<CurrencyComponent>(player);

    entt::entity item = registry.CreateEntity();
    registry.Emplace<ValueComponent>(item, ValueComponent{15});
    registry.Emplace<InventoryComponent>(player, InventoryComponent{{item}, 20});

    ShopStock stock;
    const ShopMessage message = BuildShopMessage(registry, player, stock, g_no_affixes);

    REQUIRE(message.sellable.size() == 1);
    REQUIRE(message.sellable[0].sell_value == 15);
}
