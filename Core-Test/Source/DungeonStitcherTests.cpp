#include "Engine/Dungeon/DungeonStitcher.h"

#include "Engine/Dungeon/DungeonError.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <unordered_map>
#include <unordered_set>

namespace {

using namespace psr;

// Prefab id for a small synthetic Forest-shaped test fixture: a single floor
// tile every cell stamps, sockets living as piece data rather than as their
// own stamped prefab. All sockets share the "corridor" tag (both as `tags`
// and `connects_to_tags`) so pieces chain freely under the one-way-OR match
// rule. Throwaway test fixtures only, per CLAUDE.md's carve-out -- not real
// Forest content.
constexpr std::uint32_t kFloorPrefab = 1;

DungeonPiece MakePiece(std::uint32_t id, PieceCategory category, std::vector<std::pair<Vec2, EdgeDirection>> socket_cells)
{
    DungeonPiece piece;
    piece.id = id;
    piece.id_string = "test." + std::to_string(id);
    piece.area_tag = "Forest";
    piece.category = category;

    std::unordered_map<std::int64_t, PieceCell> cells;
    auto cell_for = [&](Vec2 offset) -> PieceCell&
    {
        const std::int64_t key = (static_cast<std::int64_t>(offset.x) << 32) | static_cast<std::uint32_t>(offset.y);
        auto [it, inserted] = cells.try_emplace(key);
        if (inserted)
        {
            it->second.offset = offset;
            PieceCellPrefab floor;
            floor.prefab_id = kFloorPrefab;
            it->second.prefabs.push_back(floor);
        }
        return it->second;
    };

    // Every piece in this fixture is a straight 1xN corridor of floor cells
    // from (0,0) to the highest offset referenced by socket_cells, so a
    // "room" is just a longer corridor -- footprint shape doesn't matter for
    // these tests, only socket placement/tag-matching/connectivity do.
    for (const auto& [offset, edge] : socket_cells)
    {
        cell_for(offset); // ensure the floor cell under this socket exists
        PieceSocket socket;
        socket.cell_offset = offset;
        socket.edge = edge;
        socket.tags = {"corridor"};
        socket.connects_to_tags = {"corridor"};
        socket.fallback_prefab_id = kFloorPrefab;
        piece.sockets.push_back(socket);
    }

    for (auto& [key, cell] : cells)
        piece.cells.push_back(std::move(cell));
    return piece;
}

// A small pool: one Entrance (single socket, East), one Exit (single socket,
// West), and a two-ended Corridor (West socket at (0,0), East socket at
// (1,0)) that chains freely -- entrance -> corridor* -> exit is always
// reachable via the "corridor" tag shared by every socket.
PieceLibrary MakeTestLibrary()
{
    std::vector<DungeonPiece> pieces;
    pieces.push_back(MakePiece(100, PieceCategory::Entrance, {{Vec2{0, 0}, EdgeDirection::East}}));
    pieces.push_back(MakePiece(200, PieceCategory::Exit, {{Vec2{0, 0}, EdgeDirection::West}}));
    pieces.push_back(MakePiece(
        300, PieceCategory::Corridor, {{Vec2{0, 0}, EdgeDirection::West}, {Vec2{1, 0}, EdgeDirection::East}}));
    return PieceLibrary{std::move(pieces)};
}

Dungeon MakeTestDungeon(int room_min, int room_max, int loop_min, int loop_max)
{
    Dungeon dungeon;
    dungeon.area_tag = "Forest";
    dungeon.room_count_min = room_min;
    dungeon.room_count_max = room_max;
    dungeon.loopback_count_min = loop_min;
    dungeon.loopback_count_max = loop_max;
    dungeon.pieces.push_back(DungeonPieceRef{100, 1.0f, 1});
    dungeon.pieces.push_back(DungeonPieceRef{200, 1.0f, 1});
    dungeon.pieces.push_back(DungeonPieceRef{300, 1.0f, 0});
    return dungeon;
}

std::vector<bool> BuildReachability(const DungeonLayout& layout)
{
    std::vector<std::vector<std::size_t>> adjacency(layout.pieces.size());
    for (const SocketConnection& connection : layout.connections)
    {
        adjacency[connection.piece_a].push_back(connection.piece_b);
        adjacency[connection.piece_b].push_back(connection.piece_a);
    }
    std::vector<bool> visited(layout.pieces.size(), false);
    std::vector<std::size_t> stack{0};
    visited[0] = true;
    while (!stack.empty())
    {
        const std::size_t node = stack.back();
        stack.pop_back();
        for (std::size_t neighbor : adjacency[node])
            if (!visited[neighbor])
            {
                visited[neighbor] = true;
                stack.push_back(neighbor);
            }
    }
    return visited;
}

} // namespace

TEST_CASE("GenerateDungeon places exactly one Entrance and one Exit, fully connected", "[DungeonStitcher]")
{
    PieceLibrary library = MakeTestLibrary();
    Dungeon dungeon = MakeTestDungeon(4, 6, 0, 0);

    DungeonLayout layout = GenerateDungeon(dungeon, library, 12345);

    REQUIRE(layout.pieces.front().piece_id == 100);

    int entrance_count = 0, exit_count = 0;
    for (const PlacedPiece& piece : layout.pieces)
    {
        if (piece.piece_id == 100)
            ++entrance_count;
        if (piece.piece_id == 200)
            ++exit_count;
    }
    REQUIRE(entrance_count == 1);
    REQUIRE(exit_count == 1);

    std::vector<bool> reachable = BuildReachability(layout);
    for (bool visited : reachable)
        REQUIRE(visited);
}

TEST_CASE("GenerateDungeon never overlaps two pieces' cells", "[DungeonStitcher]")
{
    PieceLibrary library = MakeTestLibrary();
    Dungeon dungeon = MakeTestDungeon(5, 8, 0, 1);

    DungeonLayout layout = GenerateDungeon(dungeon, library, 777);

    std::unordered_set<std::int64_t> occupied;
    std::size_t total_cells = 0;
    for (const PlacedPiece& placed : layout.pieces)
    {
        const DungeonPiece* piece = library.Find(placed.piece_id);
        REQUIRE(piece != nullptr);
        for (const PieceCell& cell : piece->cells)
        {
            const Vec2 world = placed.world_offset + cell.offset;
            const std::int64_t key = (static_cast<std::int64_t>(world.x) << 32) | static_cast<std::uint32_t>(world.y);
            occupied.insert(key);
            ++total_cells;
        }
    }
    REQUIRE(occupied.size() == total_cells);
}

TEST_CASE("GenerateDungeon respects a piece ref's max_occurrences", "[DungeonStitcher]")
{
    PieceLibrary library = MakeTestLibrary();
    Dungeon dungeon = MakeTestDungeon(6, 6, 0, 0);
    // Cap the corridor ref at 2 uses -- with a 6-room target (entrance+exit+4
    // corridors would be needed otherwise), generation must fall short of the
    // target rather than exceed the cap.
    for (DungeonPieceRef& ref : dungeon.pieces)
        if (ref.piece_id == 300)
            ref.max_occurrences = 2;

    DungeonLayout layout = GenerateDungeon(dungeon, library, 42);

    int corridor_count = 0;
    for (const PlacedPiece& piece : layout.pieces)
        if (piece.piece_id == 300)
            ++corridor_count;
    REQUIRE(corridor_count <= 2);
}

TEST_CASE("GenerateDungeon is deterministic for a fixed seed", "[DungeonStitcher]")
{
    PieceLibrary library = MakeTestLibrary();
    Dungeon dungeon = MakeTestDungeon(4, 7, 0, 1);

    DungeonLayout a = GenerateDungeon(dungeon, library, 999);
    DungeonLayout b = GenerateDungeon(dungeon, library, 999);

    REQUIRE(a.pieces.size() == b.pieces.size());
    for (std::size_t i = 0; i < a.pieces.size(); ++i)
    {
        REQUIRE(a.pieces[i].piece_id == b.pieces[i].piece_id);
        REQUIRE(a.pieces[i].world_offset == b.pieces[i].world_offset);
    }
}

TEST_CASE("GenerateDungeon places a solvable lock: its key is reachable without crossing it", "[DungeonStitcher]")
{
    PieceLibrary library = MakeTestLibrary();
    Dungeon dungeon = MakeTestDungeon(6, 8, 0, 0);
    dungeon.locks.push_back(DungeonLockConfig{"red_key", 1});

    DungeonLayout layout = GenerateDungeon(dungeon, library, 2024);

    REQUIRE(layout.locks.size() == 1);
    const LockAnnotation& lock = layout.locks.front();

    // Removing the locked edge must disconnect entrance (0) from exit.
    std::vector<std::vector<std::size_t>> adjacency(layout.pieces.size());
    std::vector<std::pair<std::size_t, std::size_t>> edges;
    for (const SocketConnection& connection : layout.connections)
        edges.emplace_back(connection.piece_a, connection.piece_b);

    auto reachable_excluding = [&](std::size_t excluded_a, std::size_t excluded_b)
    {
        std::vector<std::vector<std::size_t>> adj(layout.pieces.size());
        for (const auto& [a, b] : edges)
        {
            if ((a == excluded_a && b == excluded_b) || (a == excluded_b && b == excluded_a))
                continue;
            adj[a].push_back(b);
            adj[b].push_back(a);
        }
        std::vector<bool> visited(layout.pieces.size(), false);
        std::vector<std::size_t> stack{0};
        visited[0] = true;
        while (!stack.empty())
        {
            std::size_t node = stack.back();
            stack.pop_back();
            for (std::size_t neighbor : adj[node])
                if (!visited[neighbor])
                {
                    visited[neighbor] = true;
                    stack.push_back(neighbor);
                }
        }
        return visited;
    };

    std::size_t exit_index = 0;
    for (std::size_t i = 0; i < layout.pieces.size(); ++i)
        if (layout.pieces[i].piece_id == 200)
            exit_index = i;

    std::vector<bool> without_lock = reachable_excluding(lock.edge.piece_a, lock.edge.piece_b);
    REQUIRE_FALSE(without_lock[exit_index]);
    REQUIRE(without_lock[lock.key_room_index]);
}

TEST_CASE("GenerateDungeon throws DungeonError when no Entrance piece is available", "[DungeonStitcher]")
{
    std::vector<DungeonPiece> pieces;
    pieces.push_back(MakePiece(200, PieceCategory::Exit, {{Vec2{0, 0}, EdgeDirection::West}}));
    PieceLibrary library{std::move(pieces)};

    Dungeon dungeon;
    dungeon.area_tag = "Forest";
    dungeon.pieces.push_back(DungeonPieceRef{200, 1.0f, 1});

    REQUIRE_THROWS_AS(GenerateDungeon(dungeon, library, 1), DungeonError);
}
