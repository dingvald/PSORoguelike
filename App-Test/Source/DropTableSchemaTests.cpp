#include "Items/DropTableLibraryFile.h"

#include "Engine/Persistence/JsonFile.h" // JsonFileError
#include "Items/DropTableError.h"
#include "Items/DropTableSchema.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/core/hashed_string.hpp>

#include <atomic>
#include <fstream>
#include <string>

namespace {

struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{0};
        path = std::filesystem::temp_directory_path() / "PSORoguelike-DropTableSchemaTests" /
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

} // namespace

TEST_CASE("BuildDropTableSchemaModel reflects every top-level and nested entry field", "[DropTableSchema]")
{
    const psr::DropTableSchemaModel model = psr::BuildDropTableSchemaModel();

    auto find = [&](const std::string& name) -> const psr::FieldSchema*
    {
        for (const psr::FieldSchema& field : model.fields)
            if (field.name == name)
                return &field;
        return nullptr;
    };

    CHECK(find("name")->kind == psr::FieldKind::String);

    const psr::FieldSchema* common = find("common_entries");
    REQUIRE(common != nullptr);
    CHECK(common->kind == psr::FieldKind::Array);
    const psr::FieldSchema& common_item = common->ElementSchema();
    CHECK(common_item.kind == psr::FieldKind::Object);

    auto find_child = [](const psr::FieldSchema& object, const std::string& name) -> const psr::FieldSchema*
    {
        for (const psr::FieldSchema& field : object.children)
            if (field.name == name)
                return &field;
        return nullptr;
    };
    CHECK(find_child(common_item, "item_prefab_id")->kind == psr::FieldKind::NameId);
    CHECK(find_child(common_item, "weight")->kind == psr::FieldKind::Number);
    const psr::FieldSchema* weights = find_child(common_item, "section_id_weights");
    REQUIRE(weights != nullptr);
    CHECK(weights->kind == psr::FieldKind::Object);
    CHECK(weights->children.size() == psr::kSectionIdCount);

    const psr::FieldSchema* rare = find("rare_entries");
    REQUIRE(rare != nullptr);
    CHECK(rare->kind == psr::FieldKind::Array);

    const psr::FieldSchema* guaranteed = find("guaranteed_item_ids");
    REQUIRE(guaranteed != nullptr);
    CHECK(guaranteed->kind == psr::FieldKind::Array);
    CHECK(guaranteed->ElementSchema().kind == psr::FieldKind::NameId);

    CHECK(find("rare_roll_chance_percent")->kind == psr::FieldKind::Number);
    CHECK(find("meseta_min")->kind == psr::FieldKind::Integer);
    CHECK(find("meseta_max")->kind == psr::FieldKind::Integer);
}

TEST_CASE("SaveDropTable + LoadDropTableLibrary round-trips entries, guaranteed drops, and section overrides",
          "[DropTableSchema]")
{
    psr::DropTable table;
    table.name = "Booma";
    table.rare_roll_chance_percent = 10.0f;
    table.meseta_min = 5;
    table.meseta_max = 25;
    table.guaranteed_item_ids = {entt::hashed_string::value("weapons.saber")};

    psr::DropTableEntry common;
    common.item_prefab_id = entt::hashed_string::value("materials.monomate");
    common.weight = 3.0f;
    common.section_id_weights[static_cast<std::size_t>(psr::SectionId::Redria)] = 2.0f;
    table.common_entries.push_back(common);

    psr::DropTableEntry rare;
    rare.item_prefab_id = entt::hashed_string::value("weapons.handgun");
    rare.weight = 1.0f;
    table.rare_entries.push_back(rare);

    TempDirectory temp;
    const std::filesystem::path path = temp.path / "booma.json";
    psr::SaveDropTable(path, table);

    psr::DropTableLibrary library = psr::LoadDropTableLibrary(temp.path);
    REQUIRE(library.All().size() == 1);
    const psr::DropTable& loaded = library.All().front();

    CHECK(loaded.id_string == "booma");
    CHECK(loaded.name == "Booma");
    CHECK(loaded.rare_roll_chance_percent == 10.0f);
    CHECK(loaded.meseta_min == 5);
    CHECK(loaded.meseta_max == 25);
    REQUIRE(loaded.guaranteed_item_ids.size() == 1);
    CHECK(loaded.guaranteed_item_ids[0] == entt::hashed_string::value("weapons.saber"));

    REQUIRE(loaded.common_entries.size() == 1);
    CHECK(loaded.common_entries[0].item_prefab_id == entt::hashed_string::value("materials.monomate"));
    CHECK(loaded.common_entries[0].weight == 3.0f);
    CHECK(loaded.common_entries[0].section_id_weights[static_cast<std::size_t>(psr::SectionId::Redria)] == 2.0f);
    // Every other section stays the sparse default.
    CHECK(loaded.common_entries[0].section_id_weights[static_cast<std::size_t>(psr::SectionId::Viridia)] == 1.0f);

    REQUIRE(loaded.rare_entries.size() == 1);
    CHECK(loaded.rare_entries[0].item_prefab_id == entt::hashed_string::value("weapons.handgun"));
}

TEST_CASE("LoadDropTableLibrary throws DropTableError for an unknown section id key", "[DropTableSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json", R"json({
        "schema_version": 1,
        "name": "Bad",
        "common_entries": [
            { "item_prefab_id": "x", "weight": 1.0, "section_id_weights": { "atlantis": 2.0 } }
        ]
    })json");

    REQUIRE_THROWS_AS(psr::LoadDropTableLibrary(temp.path), psr::DropTableError);
}

TEST_CASE("LoadDropTableLibrary throws JsonFileError for a schema_version mismatch", "[DropTableSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json", R"json({ "schema_version": 999, "name": "x" })json");

    REQUIRE_THROWS_AS(psr::LoadDropTableLibrary(temp.path), psr::JsonFileError);
}

TEST_CASE("LoadDropTableLibrary throws DropTableError when meseta_min exceeds meseta_max", "[DropTableSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json",
              R"json({ "schema_version": 1, "name": "Bad", "meseta_min": 50, "meseta_max": 10 })json");

    REQUIRE_THROWS_AS(psr::LoadDropTableLibrary(temp.path), psr::DropTableError);
}
