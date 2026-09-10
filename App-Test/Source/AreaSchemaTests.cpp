#include "Areas/AreaLibraryFile.h"

#include "Areas/AreaError.h"
#include "Areas/AreaSchema.h"
#include "Engine/Persistence/JsonFile.h" // JsonFileError

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{0};
        path = std::filesystem::temp_directory_path() / "PSORoguelike-AreaSchemaTests" /
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

TEST_CASE("BuildAreaSchemaModel reflects every field with the expected field kinds", "[AreaSchema]")
{
    const psr::AreaSchemaModel model = psr::BuildAreaSchemaModel();

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

    const psr::FieldSchema* tag = find("tag");
    REQUIRE(tag != nullptr);
    CHECK(tag->kind == psr::FieldKind::String);

    const psr::FieldSchema* race_id = find("race_id");
    REQUIRE(race_id != nullptr);
    CHECK(race_id->kind == psr::FieldKind::NameId);

    const psr::FieldSchema* hazard = find("hazard");
    REQUIRE(hazard != nullptr);
    CHECK(hazard->kind == psr::FieldKind::Enum);
    CHECK(hazard->enum_values == std::vector<std::string>{"none", "poison", "electric", "fire", "dark"});

    CHECK(find("floor_texture_id") != nullptr);
    CHECK(find("wall_texture_id") != nullptr);
    CHECK(find("accent_texture_id") != nullptr);

    const psr::FieldSchema* predecessor = find("unlock_predecessor_tag");
    REQUIRE(predecessor != nullptr);
    CHECK(predecessor->kind == psr::FieldKind::String);

    const psr::FieldSchema* dungeon_id_strings = find("dungeon_id_strings");
    REQUIRE(dungeon_id_strings != nullptr);
    CHECK(dungeon_id_strings->kind == psr::FieldKind::Array);
    CHECK(dungeon_id_strings->ElementSchema().kind == psr::FieldKind::String);
}

TEST_CASE("SaveArea + LoadAreaLibrary round-trips every field", "[AreaSchema]")
{
    psr::Area area;
    area.name = "Forest";
    area.tag = "Forest";
    area.race_id = 111;
    area.hazard = psr::HazardType::Poison;
    area.floor_texture_id = 222;
    area.wall_texture_id = 333;
    area.accent_texture_id = 444;
    area.unlock_predecessor_tag = "";
    area.dungeon_id_strings = {"forest_1", "forest_2"};

    TempDirectory temp;
    const std::filesystem::path path = temp.path / "forest.json";
    psr::SaveArea(path, area);

    psr::AreaLibrary library = psr::LoadAreaLibrary(temp.path);
    REQUIRE(library.All().size() == 1);
    const psr::Area& loaded = library.All().front();

    CHECK(loaded.id_string == "forest");
    CHECK(loaded.name == "Forest");
    CHECK(loaded.tag == "Forest");
    CHECK(loaded.race_id == 111);
    CHECK(loaded.hazard == psr::HazardType::Poison);
    CHECK(loaded.floor_texture_id == 222);
    CHECK(loaded.wall_texture_id == 333);
    CHECK(loaded.accent_texture_id == 444);
    CHECK(loaded.unlock_predecessor_tag.empty());
    CHECK(loaded.dungeon_id_strings == std::vector<std::string>{"forest_1", "forest_2"});

    CHECK(library.Find(loaded.id) == &library.All().front());
    CHECK(library.FindByTag("Forest") == &library.All().front());
    CHECK(library.FindByTag("Caves") == nullptr);
}

TEST_CASE("LoadAreaLibrary defaults tag to id_string when left unauthored", "[AreaSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "caves.json", R"json({ "schema_version": 1, "name": "Caves" })json");

    psr::AreaLibrary library = psr::LoadAreaLibrary(temp.path);
    REQUIRE(library.All().size() == 1);
    CHECK(library.All().front().tag == "caves");
    CHECK(library.FindByTag("caves") == &library.All().front());
}

TEST_CASE("LoadAreaLibrary throws AreaError for an unknown hazard name", "[AreaSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json", R"json({ "schema_version": 1, "name": "Bad", "hazard": "xyz" })json");

    REQUIRE_THROWS_AS(psr::LoadAreaLibrary(temp.path), psr::AreaError);
}

TEST_CASE("LoadAreaLibrary throws JsonFileError for a schema_version mismatch", "[AreaSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json", R"json({ "schema_version": 999, "name": "x" })json");

    REQUIRE_THROWS_AS(psr::LoadAreaLibrary(temp.path), psr::JsonFileError);
}
