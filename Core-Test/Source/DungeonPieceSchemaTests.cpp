#include "Engine/Dungeon/PieceLibraryFile.h"

#include "Engine/Dungeon/DungeonError.h"
#include "Engine/Dungeon/PieceSchema.h"
#include "Engine/Dungeon/PieceSchemaEmitter.h"
#include "Engine/Persistence/JsonFile.h" // JsonFileError

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <fstream>
#include <string>
#include <vector>

namespace {

// A fresh, empty subdirectory per TempDirectory instance, removed on
// destruction -- matches JsonDirectoryLoaderTests.cpp's pattern.
struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{0};
        path = std::filesystem::temp_directory_path() / "PSORoguelike-DungeonPieceSchemaTests" /
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

TEST_CASE("BuildPieceSchemaModel reflects the piece's top-level and cell/prefab fields", "[DungeonPieceSchema]")
{
    const psr::PieceSchemaModel model = psr::BuildPieceSchemaModel();

    auto find = [&](const std::string& name) -> const psr::FieldSchema*
    {
        for (const psr::FieldSchema& field : model.fields)
            if (field.name == name)
                return &field;
        return nullptr;
    };

    REQUIRE(find("name") != nullptr);
    REQUIRE(find("area_tag") != nullptr);

    const psr::FieldSchema* category = find("category");
    REQUIRE(category != nullptr);
    REQUIRE(category->kind == psr::FieldKind::Enum);
    REQUIRE(!category->enum_values.empty());

    const psr::FieldSchema* cells = find("cells");
    REQUIRE(cells != nullptr);
    REQUIRE(cells->kind == psr::FieldKind::Array);
    const psr::FieldSchema& cell_item = cells->ElementSchema();
    REQUIRE(cell_item.kind == psr::FieldKind::Object);

    bool has_offset = false, has_prefabs = false;
    for (const psr::FieldSchema& field : cell_item.children)
    {
        if (field.name == "offset")
            has_offset = true;
        if (field.name == "prefabs")
            has_prefabs = true;
    }
    REQUIRE(has_offset);
    REQUIRE(has_prefabs);

    const psr::FieldSchema* sockets = find("sockets");
    REQUIRE(sockets != nullptr);
    REQUIRE(sockets->kind == psr::FieldKind::Array);
    const psr::FieldSchema& socket_item = sockets->ElementSchema();
    REQUIRE(socket_item.kind == psr::FieldKind::Object);

    bool has_cell_offset = false, has_edge = false, has_tags = false, has_connects_to_tags = false,
         has_fallback_prefab_id = false;
    for (const psr::FieldSchema& field : socket_item.children)
    {
        if (field.name == "cell_offset")
            has_cell_offset = true;
        if (field.name == "edge")
            has_edge = true;
        if (field.name == "tags")
            has_tags = true;
        if (field.name == "connects_to_tags")
            has_connects_to_tags = true;
        if (field.name == "fallback_prefab_id")
            has_fallback_prefab_id = true;
    }
    REQUIRE(has_cell_offset);
    REQUIRE(has_edge);
    REQUIRE(has_tags);
    REQUIRE(has_connects_to_tags);
    REQUIRE(has_fallback_prefab_id);
}

TEST_CASE("SavePiece + LoadPieceLibrary round-trips an irregular, non-rectangular footprint",
          "[DungeonPieceSchema]")
{
    psr::DungeonPiece piece;
    piece.name = "L Corridor";
    piece.area_tag = "Forest";
    piece.category = psr::PieceCategory::Corridor;

    // A sparse, non-rectangular L-shape: (0,0),(1,0),(1,1) -- deliberately
    // not a full rectangle, to prove membership (not a fixed grid) defines
    // the footprint.
    psr::PieceCell a;
    a.offset = psr::Vec2{0, 0};
    psr::PieceCellPrefab floor_prefab;
    floor_prefab.prefab_id = 111;
    a.prefabs.push_back(floor_prefab);
    piece.cells.push_back(a);

    psr::PieceCell b;
    b.offset = psr::Vec2{1, 0};
    b.prefabs.push_back(floor_prefab);
    piece.cells.push_back(b);

    psr::PieceCell c;
    c.offset = psr::Vec2{1, 1};
    c.prefabs.push_back(floor_prefab);
    piece.cells.push_back(c);

    psr::PieceSocket socket;
    socket.cell_offset = psr::Vec2{1, 1};
    socket.edge = psr::EdgeDirection::South;
    socket.tags = {"door"};
    socket.connects_to_tags = {"door", "corridor"};
    socket.fallback_prefab_id = 333;
    piece.sockets.push_back(socket);

    TempDirectory temp;
    const std::filesystem::path path = temp.path / "forest_l_corridor.json";
    psr::SavePiece(path, piece);

    psr::PieceLibrary library = psr::LoadPieceLibrary(temp.path);
    REQUIRE(library.All().size() == 1);
    const psr::DungeonPiece& loaded = library.All().front();

    REQUIRE(loaded.id_string == "forest_l_corridor");
    REQUIRE(loaded.name == "L Corridor");
    REQUIRE(loaded.area_tag == "Forest");
    REQUIRE(loaded.category == psr::PieceCategory::Corridor);
    REQUIRE(loaded.cells.size() == 3);

    // The (1,1) cell has no (0,1) neighbour -- proving the shape stayed
    // sparse/irregular rather than being densified into a rectangle.
    bool found_missing_neighbor = true;
    for (const psr::PieceCell& cell : loaded.cells)
        if (cell.offset == psr::Vec2{0, 1})
            found_missing_neighbor = false;
    REQUIRE(found_missing_neighbor);

    const psr::PieceCell* corner = nullptr;
    for (const psr::PieceCell& cell : loaded.cells)
        if (cell.offset == psr::Vec2{1, 1})
            corner = &cell;
    REQUIRE(corner != nullptr);
    REQUIRE(corner->prefabs.size() == 1);

    REQUIRE(loaded.sockets.size() == 1);
    const psr::PieceSocket& loaded_socket = loaded.sockets.front();
    REQUIRE(loaded_socket.cell_offset == psr::Vec2{1, 1});
    REQUIRE(loaded_socket.edge == psr::EdgeDirection::South);
    REQUIRE(loaded_socket.tags == std::vector<std::string>{"door"});
    REQUIRE(loaded_socket.connects_to_tags == std::vector<std::string>{"door", "corridor"});
    REQUIRE(loaded_socket.fallback_prefab_id == 333);
}

TEST_CASE("LoadPieceLibrary throws DungeonError for an unknown category name", "[DungeonPieceSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json", R"json({ "schema_version": 1, "category": "not_a_category" })json");

    REQUIRE_THROWS_AS(psr::LoadPieceLibrary(temp.path), psr::DungeonError);
}

TEST_CASE("LoadPieceLibrary throws JsonFileError for a schema_version mismatch", "[DungeonPieceSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json", R"json({ "schema_version": 999, "name": "x" })json");

    REQUIRE_THROWS_AS(psr::LoadPieceLibrary(temp.path), psr::JsonFileError);
}
