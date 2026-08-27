#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/JsonEntityLoader.h"
#include "Engine/ECS/MaterialComponent.h"
#include "Engine/Items/MaterialApplication.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

namespace {

// Matches JsonEntityLoaderTests.cpp/StatsRaceComponentTests.cpp's
// TempDirectory pattern.
struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{0};
        path = std::filesystem::temp_directory_path() / "PSORoguelike-MaterialComponentTests" /
               ("run-" + std::to_string(counter++));
        std::filesystem::create_directories(path);
    }

    ~TempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

void WriteText(const std::filesystem::path& path, const std::string& contents)
{
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << contents;
}

std::uint32_t PrefabId(const std::string& id) { return entt::hashed_string::value(id.c_str()); }

} // namespace

TEST_CASE("MaterialComponent registers as authorable with stat (enum) + amount (integer)", "[EntitySchema]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::MaterialComponent::Register(reg);
    const psr::EntitySchemaModel model = reg.Model();

    REQUIRE(model.components.size() == 1);
    const psr::ComponentSchema& material = model.components[0];
    CHECK(material.id == "material");
    CHECK(material.authorable);
    REQUIRE(material.fields.size() == 2);
    CHECK(material.fields[0].name == "stat");
    CHECK(material.fields[0].kind == psr::FieldKind::Enum);
    CHECK(material.fields[0].enum_values ==
         std::vector<std::string>{"atp", "ata", "mst", "dfp", "evp", "lck", "max_hp"});
    CHECK(material.fields[1].name == "amount");
    CHECK(material.fields[1].kind == psr::FieldKind::Integer);
}

TEST_CASE("JsonEntityLoader round-trips a material entity", "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::MaterialComponent::Register(reg);

    TempDirectory temp;
    WriteText(temp.path / "items" / "power_material.json",
              R"json({
                  "schema_version": 1,
                  "components": {
                      "material": { "stat": "atp", "amount": 2 }
                  }
              })json");

    psr::JsonEntityLoader loader(ctx);
    REQUIRE(loader.Load(temp.path));

    entt::registry prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> prefab_ids;
    loader.Populate(prefab_registry, prefab_ids);

    auto it = prefab_ids.find(PrefabId("items.power_material"));
    REQUIRE(it != prefab_ids.end());

    REQUIRE(prefab_registry.all_of<psr::MaterialComponent>(it->second));
    const psr::MaterialComponent& material = prefab_registry.get<psr::MaterialComponent>(it->second);
    CHECK(material.stat == psr::MaterialStat::Atp);
    CHECK(material.amount == 2);
}

TEST_CASE("ApplyMaterial adds amount to the matching StatsComponent field", "[MaterialApplication]")
{
    psr::StatsComponent stats;
    psr::HealthComponent health;

    psr::ApplyMaterial(stats, health, psr::MaterialStat::Atp, 5);
    CHECK(stats.atp == 5);
    psr::ApplyMaterial(stats, health, psr::MaterialStat::Ata, 5);
    CHECK(stats.ata == 5);
    psr::ApplyMaterial(stats, health, psr::MaterialStat::Mst, 5);
    CHECK(stats.mst == 5);
    psr::ApplyMaterial(stats, health, psr::MaterialStat::Dfp, 5);
    CHECK(stats.dfp == 5);
    psr::ApplyMaterial(stats, health, psr::MaterialStat::Evp, 5);
    CHECK(stats.evp == 5);
    psr::ApplyMaterial(stats, health, psr::MaterialStat::Lck, 5);
    CHECK(stats.lck == 5);

    CHECK(health.max_hp == 0);
    CHECK(health.current_hp == 0);
}

TEST_CASE("ApplyMaterial with MaxHp raises both current_hp and max_hp", "[MaterialApplication]")
{
    psr::StatsComponent stats;
    psr::HealthComponent health;
    health.current_hp = 30;
    health.max_hp = 50;

    psr::ApplyMaterial(stats, health, psr::MaterialStat::MaxHp, 10);

    CHECK(health.max_hp == 60);
    CHECK(health.current_hp == 40); // rises with max_hp, not left behind as a relative wound
}
