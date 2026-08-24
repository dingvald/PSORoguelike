#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/JsonEntityLoader.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/RaceComponent.h"
#include "Engine/ECS/StatsComponent.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

namespace {

// Matches JsonEntityLoaderTests.cpp's TempDirectory pattern.
struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{0};
        path = std::filesystem::temp_directory_path() / "PSORoguelike-StatsRaceComponentTests" /
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

TEST_CASE("StatsComponent/RaceComponent register as authorable with the expected field kinds", "[EntitySchema]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::StatsComponent::Register(reg);
    psr::RaceComponent::Register(reg);
    const psr::EntitySchemaModel model = reg.Model();

    REQUIRE(model.components.size() == 2);

    const psr::ComponentSchema& stats = model.components[0];
    CHECK(stats.id == "stats");
    CHECK(stats.authorable);
    REQUIRE(stats.fields.size() == 6);
    for (const psr::FieldSchema& field : stats.fields)
        CHECK(field.kind == psr::FieldKind::Integer);
    CHECK(stats.fields[0].name == "atp");
    CHECK(stats.fields[1].name == "ata");
    CHECK(stats.fields[2].name == "mst");
    CHECK(stats.fields[3].name == "dfp");
    CHECK(stats.fields[4].name == "evp");
    CHECK(stats.fields[5].name == "lck");

    const psr::ComponentSchema& race = model.components[1];
    CHECK(race.id == "race");
    CHECK(race.authorable);
    REQUIRE(race.fields.size() == 1);
    CHECK(race.fields[0].name == "race_id");
    CHECK(race.fields[0].kind == psr::FieldKind::NameId);
}

TEST_CASE("JsonEntityLoader round-trips a stats+race entity, hashing race_id as a NameId", "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::StatsComponent::Register(reg);
    psr::RaceComponent::Register(reg);

    TempDirectory temp;
    WriteText(temp.path / "enemies" / "booma.json",
              R"json({
                  "schema_version": 1,
                  "components": {
                      "stats": { "atp": 12, "ata": 34, "mst": 0, "dfp": 8, "evp": 5, "lck": 3 },
                      "race": { "race_id": "native" }
                  }
              })json");

    psr::JsonEntityLoader loader(ctx);
    REQUIRE(loader.Load(temp.path));

    entt::registry prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> prefab_ids;
    loader.Populate(prefab_registry, prefab_ids);

    auto it = prefab_ids.find(PrefabId("enemies.booma"));
    REQUIRE(it != prefab_ids.end());

    REQUIRE(prefab_registry.all_of<psr::StatsComponent>(it->second));
    const psr::StatsComponent& stats = prefab_registry.get<psr::StatsComponent>(it->second);
    CHECK(stats.atp == 12);
    CHECK(stats.ata == 34);
    CHECK(stats.mst == 0);
    CHECK(stats.dfp == 8);
    CHECK(stats.evp == 5);
    CHECK(stats.lck == 3);

    REQUIRE(prefab_registry.all_of<psr::RaceComponent>(it->second));
    const std::uint32_t race_id = prefab_registry.get<psr::RaceComponent>(it->second).race_id;
    CHECK(race_id == entt::hashed_string::value("native"));

    std::optional<std::string> label = psr::NameIdRegistry::Find(race_id);
    REQUIRE(label.has_value());
    CHECK(*label == "native");
}
