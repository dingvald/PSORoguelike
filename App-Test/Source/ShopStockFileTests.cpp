#include "Shop/ShopStockFile.h"

#include "Engine/Persistence/JsonFile.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>

namespace {

struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{0};
        path = std::filesystem::temp_directory_path() / "PSORoguelike-ShopStockFileTests" /
               ("run-" + std::to_string(counter++));
        std::filesystem::create_directories(path);
    }

    ~TempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

} // namespace

TEST_CASE("ShopStock round-trips through save then load", "[ShopStockFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "shop_stock.json";

    psr::ShopStock stock;
    stock.entries.push_back(psr::ShopStockEntry{"monomate", 10});
    stock.entries.push_back(psr::ShopStockEntry{"weapons.saber", 80});

    psr::SaveShopStock(file, stock);
    REQUIRE(std::filesystem::exists(file));

    const psr::ShopStock loaded = psr::LoadShopStock(file);
    REQUIRE(loaded.entries.size() == 2);
    REQUIRE(loaded.entries[0].prefab_id_string == "monomate");
    REQUIRE(loaded.entries[0].buy_price == 10);
    REQUIRE(loaded.entries[1].prefab_id_string == "weapons.saber");
    REQUIRE(loaded.entries[1].buy_price == 80);
}

TEST_CASE("LoadShopStock throws on a missing file", "[ShopStockFile]")
{
    TempDirectory temp;
    REQUIRE_THROWS_AS(psr::LoadShopStock(temp.path / "missing.json"), psr::JsonFileError);
}

TEST_CASE("LoadShopStock throws when entries is missing", "[ShopStockFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "shop_stock.json";

    rapidjson::Document document;
    document.SetObject();
    document.AddMember("schema_version", psr::kShopStockVersion, document.GetAllocator());
    psr::WriteJsonFile(file, document);

    REQUIRE_THROWS_AS(psr::LoadShopStock(file), psr::JsonFileError);
}

TEST_CASE("SaveShopStock writes an empty entries array for an empty catalog", "[ShopStockFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "shop_stock.json";

    psr::SaveShopStock(file, psr::ShopStock{});
    const psr::ShopStock loaded = psr::LoadShopStock(file);
    REQUIRE(loaded.entries.empty());
}
