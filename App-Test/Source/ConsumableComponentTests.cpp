#include "Components/ConsumableComponent.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/JsonEntityLoader.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

namespace {

// Matches WeaponComponentTests.cpp/JsonEntityLoaderTests.cpp's TempDirectory pattern.
struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{0};
        path = std::filesystem::temp_directory_path() / "PSORoguelike-ConsumableComponentTests" /
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

TEST_CASE("Consumable component registers as authorable with the expected field kinds", "[EntitySchema]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::ConsumableComponent::Register(reg);
    const psr::EntitySchemaModel model = reg.Model();

    REQUIRE(model.components.size() == 1);

    const psr::ComponentSchema& consumable = model.components[0];
    CHECK(consumable.id == "consumable");
    CHECK(consumable.authorable);
    REQUIRE(consumable.fields.size() == 2);
    CHECK(consumable.fields[0].name == "effect");
    CHECK(consumable.fields[0].kind == psr::FieldKind::Enum);
    CHECK(consumable.fields[0].enum_values == std::vector<std::string>{"restore_hp", "restore_tp"});
    CHECK(consumable.fields[1].name == "amount");
    CHECK(consumable.fields[1].kind == psr::FieldKind::Integer);
}

TEST_CASE("JsonEntityLoader round-trips a consumable entity", "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::ConsumableComponent::Register(reg);

    TempDirectory temp;
    WriteText(temp.path / "consumables" / "monofluid.json",
              R"json({
                  "schema_version": 1,
                  "components": {
                      "consumable": { "effect": "restore_tp", "amount": 25 }
                  }
              })json");

    psr::JsonEntityLoader loader(ctx);
    REQUIRE(loader.Load(temp.path));

    entt::registry prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> prefab_ids;
    loader.Populate(prefab_registry, prefab_ids);

    auto it = prefab_ids.find(PrefabId("consumables.monofluid"));
    REQUIRE(it != prefab_ids.end());

    REQUIRE(prefab_registry.all_of<psr::ConsumableComponent>(it->second));
    const psr::ConsumableComponent& consumable = prefab_registry.get<psr::ConsumableComponent>(it->second);
    CHECK(consumable.effect == psr::ConsumableEffect::RestoreTp);
    CHECK(consumable.amount == 25);
}
