#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/JsonEntityLoader.h"
#include "Engine/ECS/RareVariantComponent.h"

#include <catch2/catch_approx.hpp>
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
        path = std::filesystem::temp_directory_path() / "PSORoguelike-RareVariantComponentTests" /
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

TEST_CASE("RareVariantComponent registers as authorable with is_rare (bool) + stat_multiplier (number)",
         "[EntitySchema]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::RareVariantComponent::Register(reg);
    const psr::EntitySchemaModel model = reg.Model();

    REQUIRE(model.components.size() == 1);
    const psr::ComponentSchema& rare = model.components[0];
    CHECK(rare.id == "rare_variant");
    CHECK(rare.authorable);
    REQUIRE(rare.fields.size() == 2);
    CHECK(rare.fields[0].name == "is_rare");
    CHECK(rare.fields[0].kind == psr::FieldKind::Boolean);
    CHECK(rare.fields[1].name == "stat_multiplier");
    CHECK(rare.fields[1].kind == psr::FieldKind::Number);
}

TEST_CASE("JsonEntityLoader round-trips a rare_variant entity", "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::RareVariantComponent::Register(reg);

    TempDirectory temp;
    WriteText(temp.path / "enemies" / "rag_rappy_rare.json",
              R"json({
                  "schema_version": 1,
                  "components": {
                      "rare_variant": { "is_rare": true, "stat_multiplier": 1.5 }
                  }
              })json");

    psr::JsonEntityLoader loader(ctx);
    REQUIRE(loader.Load(temp.path));

    entt::registry prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> prefab_ids;
    loader.Populate(prefab_registry, prefab_ids);

    auto it = prefab_ids.find(PrefabId("enemies.rag_rappy_rare"));
    REQUIRE(it != prefab_ids.end());

    REQUIRE(prefab_registry.all_of<psr::RareVariantComponent>(it->second));
    const psr::RareVariantComponent& rare = prefab_registry.get<psr::RareVariantComponent>(it->second);
    CHECK(rare.is_rare);
    CHECK(rare.stat_multiplier == Catch::Approx(1.5f));
}
