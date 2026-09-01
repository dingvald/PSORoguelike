#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/ItemComponent.h"
#include "Engine/ECS/JsonEntityLoader.h"
#include "Engine/ECS/ModComponent.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/RarityComponent.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

namespace {

// Matches JsonEntityLoaderTests.cpp/StatsRaceComponentTests.cpp's TempDirectory pattern.
struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{0};
        path = std::filesystem::temp_directory_path() / "PSORoguelike-ItemComponentTests" /
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

TEST_CASE("Armor/Item/Mod/Rarity components register as authorable with the expected field kinds", "[EntitySchema]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::ArmorComponent::Register(reg);
    psr::ItemComponent::Register(reg);
    psr::ModComponent::Register(reg);
    psr::RarityComponent::Register(reg);
    const psr::EntitySchemaModel model = reg.Model();

    REQUIRE(model.components.size() == 4);

    const psr::ComponentSchema& armor = model.components[0];
    CHECK(armor.id == "armor");
    CHECK(armor.authorable);
    REQUIRE(armor.fields.size() == 2);
    CHECK(armor.fields[0].name == "slot");
    CHECK(armor.fields[0].kind == psr::FieldKind::Enum);
    CHECK(armor.fields[0].enum_values == std::vector<std::string>{"head", "torso", "hands", "legs"});
    CHECK(armor.fields[1].name == "mod_slot_count");
    CHECK(armor.fields[1].kind == psr::FieldKind::Integer);

    const psr::ComponentSchema& item = model.components[1];
    CHECK(item.id == "item");
    CHECK(item.authorable);
    CHECK(item.is_tag);
    CHECK(item.fields.empty());

    const psr::ComponentSchema& mod = model.components[2];
    CHECK(mod.id == "mod");
    CHECK(mod.authorable);
    CHECK(mod.is_tag);
    CHECK(mod.fields.empty());

    const psr::ComponentSchema& rarity = model.components[3];
    CHECK(rarity.id == "rarity");
    CHECK(rarity.authorable);
    REQUIRE(rarity.fields.size() == 1);
    CHECK(rarity.fields[0].name == "stars");
    CHECK(rarity.fields[0].kind == psr::FieldKind::Integer);
}

TEST_CASE("JsonEntityLoader round-trips an armor entity", "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::ArmorComponent::Register(reg);
    psr::ItemComponent::Register(reg);
    psr::RarityComponent::Register(reg);

    TempDirectory temp;
    WriteText(temp.path / "items" / "armor" / "frame.json",
              R"json({
                  "schema_version": 1,
                  "components": {
                      "armor": { "slot": "torso", "mod_slot_count": 4 },
                      "item": {},
                      "rarity": { "stars": 1 }
                  }
              })json");

    psr::JsonEntityLoader loader(ctx);
    REQUIRE(loader.Load(temp.path));

    entt::registry prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> prefab_ids;
    loader.Populate(prefab_registry, prefab_ids);

    auto it = prefab_ids.find(PrefabId("items.armor.frame"));
    REQUIRE(it != prefab_ids.end());

    REQUIRE(prefab_registry.all_of<psr::ArmorComponent>(it->second));
    const psr::ArmorComponent& armor = prefab_registry.get<psr::ArmorComponent>(it->second);
    CHECK(armor.slot == psr::ArmorSlot::Torso);
    CHECK(armor.mod_slot_count == 4);
    CHECK(prefab_registry.all_of<psr::ItemComponent>(it->second));
}

TEST_CASE("JsonEntityLoader round-trips a mod entity with an empty body", "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::ItemComponent::Register(reg);
    psr::ModComponent::Register(reg);
    psr::RarityComponent::Register(reg);

    TempDirectory temp;
    WriteText(temp.path / "items" / "mods" / "power_unit.json",
              R"json({
                  "schema_version": 1,
                  "components": {
                      "item": {},
                      "mod": {},
                      "rarity": { "stars": 2 }
                  }
              })json");

    psr::JsonEntityLoader loader(ctx);
    REQUIRE(loader.Load(temp.path));

    entt::registry prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> prefab_ids;
    loader.Populate(prefab_registry, prefab_ids);

    auto it = prefab_ids.find(PrefabId("items.mods.power_unit"));
    REQUIRE(it != prefab_ids.end());
    CHECK(prefab_registry.all_of<psr::ModComponent>(it->second));
    CHECK(prefab_registry.all_of<psr::ItemComponent>(it->second));
}
