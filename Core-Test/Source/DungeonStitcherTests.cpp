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

DungeonPiece MakePiece(std::uint32_t id, PieceCategory category,
                       std::vector<std::pair<Vec2, EdgeDirection>> socket_cells)
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
    pieces.push_back(MakePiece(300, PieceCategory::Corridor,
                               {{Vec2{0, 0}, EdgeDirection::West}, {Vec2{1, 0}, EdgeDirection::East}}));
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

TEST_CASE("GenerateDungeon does not let Exit short-circuit growth before room_count_min is met", "[DungeonStitcher]")
{
    // Entrance's only socket and Exit's only socket share the "corridor" tag
    // and face each other, so Exit is a valid candidate on the very first
    // frontier pick -- if picked there, Exit's own single socket contributes
    // nothing further to the frontier, and growth would stop dead at 2 rooms
    // despite the much higher room_count_min. Corridor supply is unlimited, so
    // the target should be reachable every time.
    PieceLibrary library = MakeTestLibrary();
    Dungeon dungeon = MakeTestDungeon(20, 20, 0, 0);

    for (std::uint64_t seed = 0; seed < 100; ++seed)
    {
        INFO("seed=" << seed);
        DungeonLayout layout = GenerateDungeon(dungeon, library, seed);
        REQUIRE(layout.pieces.size() >= 20); // may run one past target while still hunting for Exit
    }
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

TEST_CASE("GenerateDungeon caps a dead-end Corridor socket with a Room from the pool", "[DungeonStitcher]")
{
    // Same Entrance/Exit/Corridor as MakeTestLibrary, plus a T-junction
    // Corridor (3 sockets off one cell) so growth actually branches -- the
    // plain 2-socket Corridor alone only ever grows a single unbranched
    // chain, which always has exactly zero leftover open sockets once Exit
    // caps it off, so no Corridor dead end (and thus nothing for Phase 1.5
    // to cap) would ever occur -- and a single-socket Room that can_rotate
    // so it fits whichever orientation a dead-end Corridor socket is left in.
    // No piece in this pool is tagged "dead_end", so this also exercises
    // BuildCapCandidatesPreferTagged's fallback-to-untagged path.
    DungeonPiece entrance = MakePiece(100, PieceCategory::Entrance, {{Vec2{0, 0}, EdgeDirection::East}});
    DungeonPiece exit = MakePiece(200, PieceCategory::Exit, {{Vec2{0, 0}, EdgeDirection::West}});
    DungeonPiece corridor =
        MakePiece(300, PieceCategory::Corridor, {{Vec2{0, 0}, EdgeDirection::West}, {Vec2{1, 0}, EdgeDirection::East}});
    corridor.can_rotate = true;
    DungeonPiece t_junction = MakePiece(
        350, PieceCategory::Corridor,
        {{Vec2{0, 0}, EdgeDirection::West}, {Vec2{0, 0}, EdgeDirection::East}, {Vec2{0, 0}, EdgeDirection::North}});
    // can_rotate on both Corridor pieces: without it, neither ever exposes a
    // South-facing socket, so the T-junction's North branch (needed_edge ==
    // South to cap it) could *only* ever be filled by a can_rotate Room --
    // making Room the sole growth candidate there regardless of its own
    // weight, and defeating this test's attempt to isolate Phase 1.5 capping
    // from ordinary Phase 1 growth.
    t_junction.can_rotate = true;
    DungeonPiece room = MakePiece(500, PieceCategory::Room, {{Vec2{0, 0}, EdgeDirection::West}});
    room.can_rotate = true;

    PieceLibrary library{std::vector<DungeonPiece>{entrance, exit, corridor, t_junction, room}};

    Dungeon dungeon = MakeTestDungeon(10, 12, 0, 0);
    dungeon.pieces.push_back(DungeonPieceRef{350, 1.0f, 0});
    // Zero weight: Room is also a valid growth candidate for any open
    // "corridor"-tagged socket (Phase 1 doesn't hold back single-socket
    // non-Exit pieces the way it holds back Exit, see BuildCandidates), and
    // place_best_candidate's weighted pick (DungeonStitcher.cpp) never
    // selects a zero-weight candidate over a positive-weight one in the same
    // pool -- so as long as Corridor/T-junction (both weight 1.0, unlimited)
    // are also always growth candidates for the same socket, growth can
    // never place this Room itself. Phase 1.5 capping is unaffected: it
    // only ever offers Room/Vault candidates to begin with, so there's no
    // competing positive-weight piece to lose to, and a zero (or otherwise
    // non-positive) total falls back to a plain uniform pick.
    dungeon.pieces.push_back(DungeonPieceRef{500, 0.0f, 0});

    bool found_capped_room = false;
    for (std::uint64_t seed = 0; seed < 200 && !found_capped_room; ++seed)
    {
        DungeonLayout layout;
        try
        {
            layout = GenerateDungeon(dungeon, library, seed);
        }
        catch (const DungeonError&)
        {
            continue;
        }

        for (const SocketConnection& connection : layout.connections)
            for (const std::size_t piece_index : {connection.piece_a, connection.piece_b})
                if (layout.pieces[piece_index].piece_id == 500)
                {
                    // The Room's other endpoint on this edge must be a
                    // Corridor -- capping only ever fires on Corridor dead
                    // ends, never Entrance/Exit/Room ones.
                    const std::size_t other_index =
                        piece_index == connection.piece_a ? connection.piece_b : connection.piece_a;
                    const std::uint32_t other_piece_id = layout.pieces[other_index].piece_id;
                    REQUIRE((other_piece_id == 300 || other_piece_id == 350));
                    found_capped_room = true;
                }

        std::vector<bool> reachable = BuildReachability(layout);
        for (bool visited : reachable)
            REQUIRE(visited);
    }
    REQUIRE(found_capped_room);
}

TEST_CASE("GenerateDungeon collapses a capped piece's own unused sockets to fallback dead ends", "[DungeonStitcher]")
{
    // Same branching setup as above, but the capping piece is a two-socket
    // Vault (West socket consumed by the Corridor connection, East socket
    // left over) -- verifies Phase 1.5 doesn't feed that leftover socket
    // into Phase 2 loopback matching or leave it unresolved, but stamps it
    // straight into layout.dead_ends with its own fallback_prefab_id, same
    // as an ordinary Phase 3 dead end.
    DungeonPiece entrance = MakePiece(100, PieceCategory::Entrance, {{Vec2{0, 0}, EdgeDirection::East}});
    DungeonPiece exit = MakePiece(200, PieceCategory::Exit, {{Vec2{0, 0}, EdgeDirection::West}});
    DungeonPiece corridor =
        MakePiece(300, PieceCategory::Corridor, {{Vec2{0, 0}, EdgeDirection::West}, {Vec2{1, 0}, EdgeDirection::East}});
    corridor.can_rotate = true;
    DungeonPiece t_junction = MakePiece(
        350, PieceCategory::Corridor,
        {{Vec2{0, 0}, EdgeDirection::West}, {Vec2{0, 0}, EdgeDirection::East}, {Vec2{0, 0}, EdgeDirection::North}});
    // can_rotate on both Corridor pieces: without it, neither ever exposes a
    // South-facing socket, so the T-junction's North branch (needed_edge ==
    // South to cap it) could *only* ever be filled by a can_rotate Room --
    // making Room the sole growth candidate there regardless of its own
    // weight, and defeating this test's attempt to isolate Phase 1.5 capping
    // from ordinary Phase 1 growth.
    t_junction.can_rotate = true;
    DungeonPiece vault =
        MakePiece(600, PieceCategory::Vault, {{Vec2{0, 0}, EdgeDirection::West}, {Vec2{1, 0}, EdgeDirection::East}});
    vault.can_rotate = true;

    PieceLibrary library{std::vector<DungeonPiece>{entrance, exit, corridor, t_junction, vault}};

    Dungeon dungeon = MakeTestDungeon(10, 12, 0, 0);
    dungeon.pieces.push_back(DungeonPieceRef{350, 1.0f, 0});
    // Zero weight -- see the capping test above for why this keeps growth
    // from ever placing the Vault directly.
    dungeon.pieces.push_back(DungeonPieceRef{600, 0.0f, 0});

    bool checked_a_capped_vault = false;
    for (std::uint64_t seed = 0; seed < 200 && !checked_a_capped_vault; ++seed)
    {
        DungeonLayout layout;
        try
        {
            layout = GenerateDungeon(dungeon, library, seed);
        }
        catch (const DungeonError&)
        {
            continue;
        }

        for (std::size_t piece_index = 0; piece_index < layout.pieces.size(); ++piece_index)
        {
            if (layout.pieces[piece_index].piece_id != 600)
                continue;

            int connection_count = 0;
            for (const SocketConnection& connection : layout.connections)
                if (connection.piece_a == piece_index || connection.piece_b == piece_index)
                    ++connection_count;
            REQUIRE(connection_count == 1); // only the socket that attached it to the Corridor

            int dead_end_count = 0;
            for (const DeadEndSocket& dead_end : layout.dead_ends)
                if (dead_end.piece_index == piece_index)
                {
                    ++dead_end_count;
                    CHECK(dead_end.fallback_prefab_id == kFloorPrefab);
                }
            REQUIRE(dead_end_count == 1); // the Vault's other socket, collapsed straight to fallback

            checked_a_capped_vault = true;
        }
    }
    REQUIRE(checked_a_capped_vault);
}

TEST_CASE("GenerateDungeon prefers a dead_end-tagged piece when capping, over an untagged alternative",
          "[DungeonStitcher]")
{
    DungeonPiece entrance = MakePiece(100, PieceCategory::Entrance, {{Vec2{0, 0}, EdgeDirection::East}});
    DungeonPiece exit = MakePiece(200, PieceCategory::Exit, {{Vec2{0, 0}, EdgeDirection::West}});
    DungeonPiece corridor =
        MakePiece(300, PieceCategory::Corridor, {{Vec2{0, 0}, EdgeDirection::West}, {Vec2{1, 0}, EdgeDirection::East}});
    corridor.can_rotate = true;
    DungeonPiece t_junction = MakePiece(
        350, PieceCategory::Corridor,
        {{Vec2{0, 0}, EdgeDirection::West}, {Vec2{0, 0}, EdgeDirection::East}, {Vec2{0, 0}, EdgeDirection::North}});
    // can_rotate on both Corridor pieces: without it, neither ever exposes a
    // South-facing socket, so the T-junction's North branch (needed_edge ==
    // South to cap it) could *only* ever be filled by a can_rotate Room --
    // making Room the sole growth candidate there regardless of its own
    // weight, and defeating this test's attempt to isolate Phase 1.5 capping
    // from ordinary Phase 1 growth.
    t_junction.can_rotate = true;

    // Two otherwise-equivalent single-socket Rooms, differing only in that
    // 501 is tagged "dead_end". Both are zero-weight -- see the capping test
    // above for why that keeps growth from ever placing either directly --
    // and weight has no bearing on the tag preference itself in any case,
    // since BuildCapCandidatesPreferTagged's tagged-vs-fallback split is a
    // hard filter, not a probabilistic one: whenever both fit the same dead
    // end, only tagged candidates are even offered to the pick.
    DungeonPiece untagged_room = MakePiece(500, PieceCategory::Room, {{Vec2{0, 0}, EdgeDirection::West}});
    untagged_room.can_rotate = true;
    DungeonPiece tagged_room = MakePiece(501, PieceCategory::Room, {{Vec2{0, 0}, EdgeDirection::West}});
    tagged_room.can_rotate = true;
    tagged_room.tags = {"dead_end"};

    PieceLibrary library{std::vector<DungeonPiece>{entrance, exit, corridor, t_junction, untagged_room, tagged_room}};

    Dungeon dungeon = MakeTestDungeon(10, 12, 0, 0);
    dungeon.pieces.push_back(DungeonPieceRef{350, 1.0f, 0});
    dungeon.pieces.push_back(DungeonPieceRef{500, 0.0f, 0});
    dungeon.pieces.push_back(DungeonPieceRef{501, 0.0f, 0});

    bool found_capped_piece = false;
    for (std::uint64_t seed = 0; seed < 200; ++seed)
    {
        DungeonLayout layout;
        try
        {
            layout = GenerateDungeon(dungeon, library, seed);
        }
        catch (const DungeonError&)
        {
            continue;
        }

        for (const SocketConnection& connection : layout.connections)
            for (const std::size_t piece_index : {connection.piece_a, connection.piece_b})
            {
                const std::uint32_t piece_id = layout.pieces[piece_index].piece_id;
                if (piece_id != 500 && piece_id != 501)
                    continue;
                const std::size_t other_index =
                    piece_index == connection.piece_a ? connection.piece_b : connection.piece_a;
                const std::uint32_t other_piece_id = layout.pieces[other_index].piece_id;
                if (other_piece_id != 300 && other_piece_id != 350)
                    continue; // a Corridor->Room/Vault edge is specifically a capping edge

                found_capped_piece = true;
                REQUIRE(piece_id == 501); // the tagged Room, never the untagged one, whenever capping fires
            }
    }
    REQUIRE(found_capped_piece);
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

TEST_CASE("GenerateDungeon never gaps or overlaps rotated/mirrored L-shaped pieces, across many seeds",
          "[DungeonStitcher]")
{
    // L-shaped, like the real test_hall2.json content this mirrors: a
    // horizontal arm and a vertical arm sharing a corner cell, authored far
    // from local origin (ApplyPieceTransform pivots on world (0,0), not the
    // piece's own bounds -- this shape and offset choice deliberately
    // stresses that). Unlike a straight piece, an L-shape's chirality
    // actually changes under mirror, so it's the shape most likely to expose
    // a mirror-specific bug; this regression-tests the DungeonEditorLayer.cpp
    // bug where the dungeon-generation preview computed
    // "placed.world_offset + cell.offset" directly, without
    // ApplyPieceTransform -- DungeonStitcher/DungeonInstantiator themselves
    // were always correct, but that separate preview renderer wasn't updated
    // when PieceTransform was introduced, so a rotated/mirrored piece's cells
    // rendered in the wrong place there even though the underlying layout
    // was fine.
    DungeonPiece entrance = MakePiece(100, PieceCategory::Entrance, {{Vec2{0, 0}, EdgeDirection::East}});
    DungeonPiece exit = MakePiece(200, PieceCategory::Exit, {{Vec2{0, 0}, EdgeDirection::West}});

    DungeonPiece corridor;
    corridor.id = 300;
    corridor.id_string = "test.300";
    corridor.area_tag = "Forest";
    corridor.category = PieceCategory::Corridor;
    corridor.can_rotate = true;
    corridor.can_mirror = true;
    for (int x = 8; x <= 10; ++x)
    {
        PieceCell cell;
        cell.offset = Vec2{x, 12};
        cell.prefabs.push_back(PieceCellPrefab{kFloorPrefab});
        corridor.cells.push_back(cell);
    }
    for (int y = 13; y <= 14; ++y)
    {
        PieceCell cell;
        cell.offset = Vec2{8, y};
        cell.prefabs.push_back(PieceCellPrefab{kFloorPrefab});
        corridor.cells.push_back(cell);
    }
    PieceSocket west_socket;
    west_socket.cell_offset = Vec2{10, 12};
    west_socket.edge = EdgeDirection::East;
    west_socket.tags = {"corridor"};
    west_socket.connects_to_tags = {"corridor"};
    corridor.sockets.push_back(west_socket);
    PieceSocket east_socket;
    east_socket.cell_offset = Vec2{8, 14};
    east_socket.edge = EdgeDirection::South;
    east_socket.tags = {"corridor"};
    east_socket.connects_to_tags = {"corridor"};
    corridor.sockets.push_back(east_socket);

    PieceLibrary library{std::vector<DungeonPiece>{entrance, exit, corridor}};

    Dungeon dungeon;
    dungeon.area_tag = "Forest";
    dungeon.room_count_min = 8;
    dungeon.room_count_max = 12;
    dungeon.loopback_count_min = 0;
    dungeon.loopback_count_max = 2;
    dungeon.pieces.push_back(DungeonPieceRef{100, 1.0f, 1});
    dungeon.pieces.push_back(DungeonPieceRef{200, 1.0f, 1});
    dungeon.pieces.push_back(DungeonPieceRef{300, 1.0f, 0});

    int generations_checked = 0;
    for (std::uint64_t seed = 0; seed < 300; ++seed)
    {
        DungeonLayout layout;
        try
        {
            layout = GenerateDungeon(dungeon, library, seed);
        }
        catch (const DungeonError&)
        {
            continue; // ran out of placement attempts for this seed -- not what this test checks
        }
        ++generations_checked;

        std::unordered_set<std::int64_t> occupied;
        std::size_t total_cells = 0;
        for (const PlacedPiece& placed : layout.pieces)
        {
            const DungeonPiece* piece = library.Find(placed.piece_id);
            REQUIRE(piece != nullptr);
            for (const PieceCell& cell : piece->cells)
            {
                const Vec2 world = placed.world_offset + ApplyPieceTransform(cell.offset, placed.transform);
                const std::int64_t key =
                    (static_cast<std::int64_t>(world.x) << 32) | static_cast<std::uint32_t>(world.y);
                occupied.insert(key);
                ++total_cells;
            }
        }
        INFO("seed=" << seed);
        REQUIRE(occupied.size() == total_cells);

        for (const SocketConnection& connection : layout.connections)
        {
            const Vec2 diff = connection.cell_b - connection.cell_a;
            const int manhattan = std::abs(diff.x) + std::abs(diff.y);
            INFO("seed=" << seed << " a=" << connection.piece_a << " b=" << connection.piece_b);
            REQUIRE(manhattan == 1);
        }
    }
    REQUIRE(generations_checked > 0); // sanity: the loop actually exercised something
}

TEST_CASE("GenerateDungeon rotates a can_rotate piece when its authored sockets don't otherwise fit",
          "[DungeonStitcher]")
{
    // Entrance's only socket faces East (so a neighbour must present a
    // West-facing socket to connect). The bridge piece is authored with
    // sockets on North and East only -- never West as-authored -- so it can
    // only ever connect to Entrance via one of its can_rotate orientations
    // (a 90 or 270 degree clockwise rotation each turn one of its two
    // sockets onto West; which one depends on the random pick, so Exit is
    // authored to accept either of the two possible leftover edges, North
    // or South, while never itself exposing East/West -- ruling out a
    // direct Entrance->Exit connection that would make the bridge's
    // rotation merely incidental rather than required for generation to
    // succeed at all).
    DungeonPiece entrance = MakePiece(100, PieceCategory::Entrance, {{Vec2{0, 0}, EdgeDirection::East}});
    DungeonPiece bridge = MakePiece(400, PieceCategory::Corridor,
                                    {{Vec2{0, 0}, EdgeDirection::North}, {Vec2{0, 0}, EdgeDirection::East}});
    bridge.can_rotate = true;
    DungeonPiece exit =
        MakePiece(200, PieceCategory::Exit, {{Vec2{0, 0}, EdgeDirection::North}, {Vec2{0, 0}, EdgeDirection::South}});

    PieceLibrary library{std::vector<DungeonPiece>{entrance, bridge, exit}};

    Dungeon dungeon;
    dungeon.area_tag = "Forest";
    dungeon.room_count_min = 3;
    dungeon.room_count_max = 3;
    dungeon.loopback_count_min = 0;
    dungeon.loopback_count_max = 0;
    dungeon.pieces.push_back(DungeonPieceRef{100, 1.0f, 1});
    dungeon.pieces.push_back(DungeonPieceRef{400, 1.0f, 1});
    dungeon.pieces.push_back(DungeonPieceRef{200, 1.0f, 1});

    DungeonLayout layout = GenerateDungeon(dungeon, library, 1);

    REQUIRE(layout.pieces.size() == 3);

    const PlacedPiece* placed_bridge = nullptr;
    int exit_count = 0;
    for (const PlacedPiece& placed : layout.pieces)
    {
        if (placed.piece_id == 400)
            placed_bridge = &placed;
        if (placed.piece_id == 200)
            ++exit_count;
    }
    REQUIRE(exit_count == 1);
    REQUIRE(placed_bridge != nullptr);
    REQUIRE(placed_bridge->transform != PieceTransform{});
    REQUIRE(placed_bridge->transform.mirrored == false);
}
