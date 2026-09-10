#include "Engine/Dungeon/DungeonLibraryFile.h"

#include "Engine/Dungeon/DungeonError.h"
#include "Engine/Dungeon/DungeonSchema.h"
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
        path = std::filesystem::temp_directory_path() / "PSORoguelike-DungeonSchemaTests" /
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

TEST_CASE("BuildDungeonSchemaModel reflects the dungeon's top-level and nested fields", "[DungeonSchema]")
{
    const psr::DungeonSchemaModel model = psr::BuildDungeonSchemaModel();

    auto find = [&](const std::string& name) -> const psr::FieldSchema*
    {
        for (const psr::FieldSchema& field : model.fields)
            if (field.name == name)
                return &field;
        return nullptr;
    };

    REQUIRE(find("name") != nullptr);
    REQUIRE(find("area_tag") != nullptr);
    REQUIRE(find("room_count_min") != nullptr);
    REQUIRE(find("room_count_max") != nullptr);
    REQUIRE(find("loopback_count_min") != nullptr);
    REQUIRE(find("loopback_count_max") != nullptr);

    const psr::FieldSchema* pieces = find("pieces");
    REQUIRE(pieces != nullptr);
    REQUIRE(pieces->kind == psr::FieldKind::Array);
    bool has_piece_id = false, has_weight = false, has_max_occurrences = false;
    for (const psr::FieldSchema& field : pieces->ElementSchema().children)
    {
        if (field.name == "piece_id")
            has_piece_id = true;
        if (field.name == "weight")
            has_weight = true;
        if (field.name == "max_occurrences")
            has_max_occurrences = true;
    }
    REQUIRE(has_piece_id);
    REQUIRE(has_weight);
    REQUIRE(has_max_occurrences);

    REQUIRE(find("lock_count") != nullptr);

    const psr::FieldSchema* unlocked_door = find("unlocked_door_prefab_id");
    REQUIRE(unlocked_door != nullptr);
    REQUIRE(unlocked_door->kind == psr::FieldKind::NameId);
    const psr::FieldSchema* locked_door = find("locked_door_prefab_id");
    REQUIRE(locked_door != nullptr);
    REQUIRE(locked_door->kind == psr::FieldKind::NameId);
    const psr::FieldSchema* switch_prefab = find("switch_prefab_id");
    REQUIRE(switch_prefab != nullptr);
    REQUIRE(switch_prefab->kind == psr::FieldKind::NameId);
}

TEST_CASE("SaveDungeon + LoadDungeonLibrary round-trips piece refs and lock/door fields", "[DungeonSchema]")
{
    psr::Dungeon dungeon;
    dungeon.name = "Forest Mission";
    dungeon.area_tag = "Forest";
    dungeon.room_count_min = 15;
    dungeon.room_count_max = 20;
    dungeon.loopback_count_min = 1;
    dungeon.loopback_count_max = 3;
    dungeon.pieces.push_back(psr::DungeonPieceRef{111, 2.5f, 4});
    dungeon.pieces.push_back(psr::DungeonPieceRef{222, 1.0f, 0});
    dungeon.lock_count = 2;
    dungeon.unlocked_door_prefab_id = 333;
    dungeon.locked_door_prefab_id = 444;
    dungeon.switch_prefab_id = 555;

    TempDirectory temp;
    const std::filesystem::path path = temp.path / "forest_mission.json";
    psr::SaveDungeon(path, dungeon);

    psr::DungeonLibrary library = psr::LoadDungeonLibrary(temp.path);
    REQUIRE(library.All().size() == 1);
    const psr::Dungeon& loaded = library.All().front();

    REQUIRE(loaded.id_string == "forest_mission");
    REQUIRE(loaded.name == "Forest Mission");
    REQUIRE(loaded.area_tag == "Forest");
    REQUIRE(loaded.room_count_min == 15);
    REQUIRE(loaded.room_count_max == 20);
    REQUIRE(loaded.loopback_count_min == 1);
    REQUIRE(loaded.loopback_count_max == 3);
    REQUIRE(loaded.pieces.size() == 2);
    REQUIRE(loaded.pieces[0].piece_id == 111);
    REQUIRE(loaded.pieces[0].weight == 2.5f);
    REQUIRE(loaded.pieces[0].max_occurrences == 4);
    REQUIRE(loaded.lock_count == 2);
    REQUIRE(loaded.unlocked_door_prefab_id == 333);
    REQUIRE(loaded.locked_door_prefab_id == 444);
    REQUIRE(loaded.switch_prefab_id == 555);
}

TEST_CASE("ReadDungeonBody throws DungeonError when room_count_min exceeds room_count_max", "[DungeonSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json",
              R"json({ "schema_version": 1, "room_count_min": 20, "room_count_max": 10 })json");

    REQUIRE_THROWS_AS(psr::LoadDungeonLibrary(temp.path), psr::DungeonError);
}

TEST_CASE("LoadDungeonLibrary throws JsonFileError for a schema_version mismatch", "[DungeonSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json", R"json({ "schema_version": 999, "name": "x" })json");

    REQUIRE_THROWS_AS(psr::LoadDungeonLibrary(temp.path), psr::JsonFileError);
}
