#include "Engine/Items/DropTableLibraryFile.h"

#include "Engine/Items/DropTableError.h"
#include "Engine/Items/DropTableSchema.h"
#include "Engine/Persistence/JsonFile.h" // JsonFileError

#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("BuildDropTableSchemaModel reflects every field with the expected kind", "[DropTableSchema]")
{
    const psr::DropTableSchemaModel model = psr::BuildDropTableSchemaModel();

    auto find = [&](const std::string& name) -> const psr::FieldSchema*
    {
        for (const psr::FieldSchema& field : model.fields)
            if (field.name == name)
                return &field;
        return nullptr;
    };

    const psr::FieldSchema* name = find("name");
    REQUIRE(name != nullptr);
    CHECK(name->kind == psr::FieldKind::String);

    const psr::FieldSchema* common = find("common_entries");
    REQUIRE(common != nullptr);
    CHECK(common->kind == psr::FieldKind::Array);
    REQUIRE(common->children.size() == 1);
    CHECK(common->children.front().kind == psr::FieldKind::Object);

    const psr::FieldSchema* rare = find("rare_entries");
    REQUIRE(rare != nullptr);
    CHECK(rare->kind == psr::FieldKind::Array);

    const psr::FieldSchema* rare_chance = find("rare_chance_percent");
    REQUIRE(rare_chance != nullptr);
    CHECK(rare_chance->kind == psr::FieldKind::Integer);

    const psr::FieldSchema* guaranteed = find("boss_guaranteed_rare");
    REQUIRE(guaranteed != nullptr);
    CHECK(guaranteed->kind == psr::FieldKind::Boolean);

    const psr::FieldSchema* meseta_min = find("meseta_min");
    REQUIRE(meseta_min != nullptr);
    CHECK(meseta_min->kind == psr::FieldKind::Integer);

    const psr::FieldSchema* meseta_max = find("meseta_max");
    REQUIRE(meseta_max != nullptr);
    CHECK(meseta_max->kind == psr::FieldKind::Integer);
}

TEST_CASE("SaveDropTable + LoadDropTableLibrary round-trips every field", "[DropTableSchema]")
{
    psr::DropTable table;
    table.name = "Booma";
    table.common_entries = {{/*item_prefab_id=*/0, /*weight=*/5, psr::SectionId::None}};
    table.rare_entries = {{/*item_prefab_id=*/0, /*weight=*/1, psr::SectionId::Redria}};
    table.common_entries.front().item_prefab_id = 111;
    table.rare_entries.front().item_prefab_id = 222;
    table.rare_chance_percent = 3;
    table.boss_guaranteed_rare = false;
    table.meseta_min = 5;
    table.meseta_max = 50;

    TempDirectory temp;
    const std::filesystem::path path = temp.path / "booma.json";
    psr::SaveDropTable(path, table);

    psr::DropTableLibrary library = psr::LoadDropTableLibrary(temp.path);
    REQUIRE(library.All().size() == 1);
    const psr::DropTable& loaded = library.All().front();

    CHECK(loaded.id_string == "booma");
    CHECK(loaded.name == "Booma");
    REQUIRE(loaded.common_entries.size() == 1);
    CHECK(loaded.common_entries.front().item_prefab_id == 111);
    CHECK(loaded.common_entries.front().weight == 5);
    CHECK(loaded.common_entries.front().favored_section_id == psr::SectionId::None);
    REQUIRE(loaded.rare_entries.size() == 1);
    CHECK(loaded.rare_entries.front().item_prefab_id == 222);
    CHECK(loaded.rare_entries.front().favored_section_id == psr::SectionId::Redria);
    CHECK(loaded.rare_chance_percent == 3);
    CHECK(loaded.boss_guaranteed_rare == false);
    CHECK(loaded.meseta_min == 5);
    CHECK(loaded.meseta_max == 50);
}

TEST_CASE("LoadDropTableLibrary throws DropTableError for an unknown favored_section_id name", "[DropTableSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json",
              R"json({ "schema_version": 1, "name": "Bad", "common_entries": [
                  { "item_prefab_id": 1, "weight": 1, "favored_section_id": "xyz" }
              ] })json");

    REQUIRE_THROWS_AS(psr::LoadDropTableLibrary(temp.path), psr::DropTableError);
}

TEST_CASE("LoadDropTableLibrary throws JsonFileError for a schema_version mismatch", "[DropTableSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json", R"json({ "schema_version": 999, "name": "x" })json");

    REQUIRE_THROWS_AS(psr::LoadDropTableLibrary(temp.path), psr::JsonFileError);
}
