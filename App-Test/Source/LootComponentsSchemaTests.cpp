#include "Components/CurrencyComponent.h"
#include "Components/DropTableComponent.h"
#include "Components/SectionIdComponent.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/JsonEntityLoader.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Items/SectionId.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

namespace {

// Matches StatsRaceComponentTests.cpp's own TempDirectory pattern.
struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{0};
        path = std::filesystem::temp_directory_path() / "PSORoguelike-LootComponentsSchemaTests" /
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

TEST_CASE("SectionIdComponent/DropTableComponent/CurrencyComponent register as authorable", "[EntitySchema]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::SectionIdComponent::Register(reg);
    psr::DropTableComponent::Register(reg);
    psr::CurrencyComponent::Register(reg);
    const psr::EntitySchemaModel model = reg.Model();

    REQUIRE(model.components.size() == 3);

    const psr::ComponentSchema& section_id = model.components[0];
    CHECK(section_id.id == "section_id");
    CHECK(section_id.authorable);
    REQUIRE(section_id.fields.size() == 1);
    CHECK(section_id.fields[0].name == "section_id");
    CHECK(section_id.fields[0].kind == psr::FieldKind::Enum);
    CHECK(section_id.fields[0].enum_values.size() == psr::kSectionIdCount);

    const psr::ComponentSchema& drop_table = model.components[1];
    CHECK(drop_table.id == "drop_table");
    CHECK(drop_table.authorable);
    REQUIRE(drop_table.fields.size() == 1);
    CHECK(drop_table.fields[0].name == "drop_table_id");
    CHECK(drop_table.fields[0].kind == psr::FieldKind::NameId);

    const psr::ComponentSchema& currency = model.components[2];
    CHECK(currency.id == "currency");
    CHECK(currency.authorable);
    REQUIRE(currency.fields.size() == 1);
    CHECK(currency.fields[0].name == "meseta");
    CHECK(currency.fields[0].kind == psr::FieldKind::Integer);
}

TEST_CASE("JsonEntityLoader round-trips section_id/drop_table/currency, hashing drop_table_id as a NameId",
          "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::SectionIdComponent::Register(reg);
    psr::DropTableComponent::Register(reg);
    psr::CurrencyComponent::Register(reg);

    TempDirectory temp;
    WriteText(temp.path / "player.json",
              R"json({
                  "schema_version": 1,
                  "components": {
                      "section_id": { "section_id": "redria" },
                      "drop_table": { "drop_table_id": "booma" },
                      "currency": { "meseta": 0 }
                  }
              })json");

    psr::JsonEntityLoader loader(ctx);
    REQUIRE(loader.Load(temp.path));

    entt::registry prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> prefab_ids;
    loader.Populate(prefab_registry, prefab_ids);

    auto it = prefab_ids.find(PrefabId("player"));
    REQUIRE(it != prefab_ids.end());

    REQUIRE(prefab_registry.all_of<psr::SectionIdComponent>(it->second));
    CHECK(prefab_registry.get<psr::SectionIdComponent>(it->second).section_id == psr::SectionId::Redria);

    REQUIRE(prefab_registry.all_of<psr::DropTableComponent>(it->second));
    const std::uint32_t drop_table_id = prefab_registry.get<psr::DropTableComponent>(it->second).drop_table_id;
    CHECK(drop_table_id == entt::hashed_string::value("booma"));

    std::optional<std::string> label = psr::NameIdRegistry::Find(drop_table_id);
    REQUIRE(label.has_value());
    CHECK(*label == "booma");

    REQUIRE(prefab_registry.all_of<psr::CurrencyComponent>(it->second));
    CHECK(prefab_registry.get<psr::CurrencyComponent>(it->second).meseta == 0);
}
