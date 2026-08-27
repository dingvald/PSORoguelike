#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/JsonEntityLoader.h"

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
        path = std::filesystem::temp_directory_path() / "PSORoguelike-HealthComponentTests" /
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

TEST_CASE("HealthComponent registers as authorable with two integer fields", "[EntitySchema]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::HealthComponent::Register(reg);
    const psr::EntitySchemaModel model = reg.Model();

    REQUIRE(model.components.size() == 1);
    const psr::ComponentSchema& health = model.components[0];
    CHECK(health.id == "health");
    CHECK(health.authorable);
    REQUIRE(health.fields.size() == 2);
    CHECK(health.fields[0].name == "current_hp");
    CHECK(health.fields[0].kind == psr::FieldKind::Integer);
    CHECK(health.fields[1].name == "max_hp");
    CHECK(health.fields[1].kind == psr::FieldKind::Integer);
}

TEST_CASE("JsonEntityLoader round-trips a health entity", "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::HealthComponent::Register(reg);

    TempDirectory temp;
    WriteText(temp.path / "enemies" / "booma.json",
              R"json({
                  "schema_version": 1,
                  "components": {
                      "health": { "current_hp": 20, "max_hp": 30 }
                  }
              })json");

    psr::JsonEntityLoader loader(ctx);
    REQUIRE(loader.Load(temp.path));

    entt::registry prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> prefab_ids;
    loader.Populate(prefab_registry, prefab_ids);

    auto it = prefab_ids.find(PrefabId("enemies.booma"));
    REQUIRE(it != prefab_ids.end());

    REQUIRE(prefab_registry.all_of<psr::HealthComponent>(it->second));
    const psr::HealthComponent& health = prefab_registry.get<psr::HealthComponent>(it->second);
    CHECK(health.current_hp == 20);
    CHECK(health.max_hp == 30);
}
