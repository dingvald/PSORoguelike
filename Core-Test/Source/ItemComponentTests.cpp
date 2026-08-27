#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/JsonEntityLoader.h"
#include "Engine/ECS/ModComponent.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/RarityComponent.h"
#include "Engine/ECS/StatsComponent.h"
#include "Engine/ECS/WeaponComponent.h"

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

TEST_CASE("Weapon/Armor/Mod/Rarity components register as authorable with the expected field kinds", "[EntitySchema]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::ArmorComponent::Register(reg);
    psr::ModComponent::Register(reg);
    psr::RarityComponent::Register(reg);
    psr::WeaponComponent::Register(reg);
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

    const psr::ComponentSchema& mod = model.components[1];
    CHECK(mod.id == "mod");
    CHECK(mod.authorable);
    CHECK(mod.is_tag);
    CHECK(mod.fields.empty());

    const psr::ComponentSchema& rarity = model.components[2];
    CHECK(rarity.id == "rarity");
    CHECK(rarity.authorable);
    REQUIRE(rarity.fields.size() == 1);
    CHECK(rarity.fields[0].name == "stars");
    CHECK(rarity.fields[0].kind == psr::FieldKind::Integer);

    const psr::ComponentSchema& weapon = model.components[3];
    CHECK(weapon.id == "weapon");
    CHECK(weapon.authorable);
    REQUIRE(weapon.fields.size() == 7);
    CHECK(weapon.fields[0].name == "range_shape");
    CHECK(weapon.fields[0].kind == psr::FieldKind::Enum);
    CHECK(weapon.fields[0].enum_values ==
          std::vector<std::string>{"single_target", "cone_3", "surrounding", "line"});
    CHECK(weapon.fields[1].name == "range");
    CHECK(weapon.fields[1].kind == psr::FieldKind::Integer);
    CHECK(weapon.fields[2].name == "hits_per_turn");
    CHECK(weapon.fields[2].kind == psr::FieldKind::Integer);
    CHECK(weapon.fields[3].name == "grind_level");
    CHECK(weapon.fields[3].kind == psr::FieldKind::Integer);
    CHECK(weapon.fields[4].name == "prefix_affix_id");
    CHECK(weapon.fields[4].kind == psr::FieldKind::NameId);
    CHECK(weapon.fields[5].name == "suffix_affix_id");
    CHECK(weapon.fields[5].kind == psr::FieldKind::NameId);
    CHECK(weapon.fields[6].name == "race_bonuses");
    CHECK(weapon.fields[6].kind == psr::FieldKind::Array);
    const psr::FieldSchema& race_bonus_item = weapon.fields[6].ElementSchema();
    CHECK(race_bonus_item.kind == psr::FieldKind::Object);
    REQUIRE(race_bonus_item.children.size() == 2);
    CHECK(race_bonus_item.children[0].name == "race_id");
    CHECK(race_bonus_item.children[0].kind == psr::FieldKind::NameId);
    CHECK(race_bonus_item.children[1].name == "bonus_percent");
    CHECK(race_bonus_item.children[1].kind == psr::FieldKind::Integer);
}

TEST_CASE("JsonEntityLoader round-trips a weapon entity, including race_bonuses and a non-default range_shape",
          "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::StatsComponent::Register(reg);
    psr::RarityComponent::Register(reg);
    psr::WeaponComponent::Register(reg);

    TempDirectory temp;
    WriteText(temp.path / "items" / "weapons" / "handgun.json",
              R"json({
                  "schema_version": 1,
                  "components": {
                      "stats": { "atp": 10, "ata": 40, "mst": 0, "dfp": 0, "evp": 0, "lck": 0 },
                      "rarity": { "stars": 3 },
                      "weapon": {
                          "range_shape": "line",
                          "range": 5,
                          "hits_per_turn": 1,
                          "grind_level": 2,
                          "prefix_affix_id": "power",
                          "suffix_affix_id": 0,
                          "race_bonuses": [
                              { "race_id": "native", "bonus_percent": 15 },
                              { "race_id": "machine", "bonus_percent": 5 }
                          ]
                      }
                  }
              })json");

    psr::JsonEntityLoader loader(ctx);
    REQUIRE(loader.Load(temp.path));

    entt::registry prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> prefab_ids;
    loader.Populate(prefab_registry, prefab_ids);

    auto it = prefab_ids.find(PrefabId("items.weapons.handgun"));
    REQUIRE(it != prefab_ids.end());

    REQUIRE(prefab_registry.all_of<psr::RarityComponent>(it->second));
    CHECK(prefab_registry.get<psr::RarityComponent>(it->second).stars == 3);

    REQUIRE(prefab_registry.all_of<psr::WeaponComponent>(it->second));
    const psr::WeaponComponent& weapon = prefab_registry.get<psr::WeaponComponent>(it->second);
    CHECK(weapon.range_shape == psr::WeaponRangeShape::Line);
    CHECK(weapon.range == 5);
    CHECK(weapon.hits_per_turn == 1);
    CHECK(weapon.grind_level == 2);
    CHECK(weapon.prefix_affix_id == entt::hashed_string::value("power"));
    CHECK(weapon.suffix_affix_id == 0);
    REQUIRE(weapon.race_bonuses.size() == 2);
    CHECK(weapon.race_bonuses[0].race_id == entt::hashed_string::value("native"));
    CHECK(weapon.race_bonuses[0].bonus_percent == 15);
    CHECK(weapon.race_bonuses[1].race_id == entt::hashed_string::value("machine"));
    CHECK(weapon.race_bonuses[1].bonus_percent == 5);
}

TEST_CASE("JsonEntityLoader round-trips an armor entity", "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::StatsComponent::Register(reg);
    psr::ArmorComponent::Register(reg);
    psr::RarityComponent::Register(reg);

    TempDirectory temp;
    WriteText(temp.path / "items" / "armor" / "frame.json",
              R"json({
                  "schema_version": 1,
                  "components": {
                      "stats": { "atp": 0, "ata": 0, "mst": 0, "dfp": 20, "evp": 5, "lck": 0 },
                      "armor": { "slot": "torso", "mod_slot_count": 4 },
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
}

TEST_CASE("JsonEntityLoader round-trips a mod entity with an empty body", "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::StatsComponent::Register(reg);
    psr::ModComponent::Register(reg);
    psr::RarityComponent::Register(reg);

    TempDirectory temp;
    WriteText(temp.path / "items" / "mods" / "power_unit.json",
              R"json({
                  "schema_version": 1,
                  "components": {
                      "stats": { "atp": 5, "ata": 0, "mst": 0, "dfp": 0, "evp": 0, "lck": 0 },
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
}
