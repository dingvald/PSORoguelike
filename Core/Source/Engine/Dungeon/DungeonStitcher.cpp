#include "Engine/Dungeon/DungeonStitcher.h"

#include "Engine/Dungeon/DungeonError.h"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <queue>
#include <random>
#include <unordered_map>
#include <unordered_set>

namespace psr {

namespace {

    // A piece's socket resolved to its piece-local position -- computed once
    // per referenced piece and cached (see socket_cache below), since a piece
    // may be considered as a growth candidate many times.
    struct ResolvedSocket
    {
        Vec2 local_cell;
        EdgeDirection edge;
        SocketInfo info;
    };

    // A socket on an already-placed piece, not yet connected to anything.
    struct OpenSocket
    {
        std::size_t piece_index;
        Vec2 world_cell;
        EdgeDirection edge;
        SocketInfo info;
    };

    std::int64_t PackCell(Vec2 v)
    {
        return (static_cast<std::int64_t>(v.x) << 32) | static_cast<std::uint32_t>(v.y);
    }

    bool SharesTag(const std::vector<std::string>& a, const std::vector<std::string>& b)
    {
        for (const std::string& tag : a)
            for (const std::string& other : b)
                if (tag == other)
                    return true;
        return false;
    }

    std::vector<ResolvedSocket> ResolveSockets(const DungeonPiece& piece, const SocketLookup& lookup)
    {
        std::vector<ResolvedSocket> sockets;
        for (const PieceCell& cell : piece.cells)
            for (const PieceCellPrefab& prefab : cell.prefabs)
                if (std::optional<SocketInfo> info = lookup(prefab.prefab_id))
                    sockets.push_back(ResolvedSocket{cell.offset, prefab.edge, std::move(*info)});
        return sockets;
    }

    // One growth candidate: a dungeon-pool piece ref plus which of its own
    // resolved sockets would connect to the open socket being grown from.
    struct Candidate
    {
        const DungeonPieceRef* ref;
        const DungeonPiece* piece;
        std::size_t socket_index;
    };

    std::vector<Candidate> BuildCandidates(const Dungeon& dungeon, const PieceLibrary& library,
                                           const std::unordered_map<std::uint32_t, std::vector<ResolvedSocket>>& socket_cache,
                                           const std::unordered_map<std::uint32_t, int>& occurrence_count,
                                           const OpenSocket& open, bool exit_already_placed)
    {
        std::vector<Candidate> candidates;
        const EdgeDirection needed_edge = OppositeEdge(open.edge);

        for (const DungeonPieceRef& ref : dungeon.pieces)
        {
            const DungeonPiece* piece = library.Find(ref.piece_id);
            if (!piece || piece->area_tag != dungeon.area_tag)
                continue;
            if (piece->category == PieceCategory::Entrance)
                continue; // placed exactly once, before growth starts
            if (piece->category == PieceCategory::Exit && exit_already_placed)
                continue; // exactly one Exit ever placed
            if (ref.max_occurrences != 0)
            {
                auto it = occurrence_count.find(ref.piece_id);
                if (it != occurrence_count.end() && it->second >= ref.max_occurrences)
                    continue;
            }

            auto cached = socket_cache.find(ref.piece_id);
            if (cached == socket_cache.end())
                continue;
            for (std::size_t i = 0; i < cached->second.size(); ++i)
            {
                const ResolvedSocket& candidate_socket = cached->second[i];
                if (candidate_socket.edge == needed_edge && SharesTag(open.info.tags, candidate_socket.info.tags))
                    candidates.push_back(Candidate{&ref, piece, i});
            }
        }
        return candidates;
    }

    // Reachability from `start`, treating any connection whose index is true
    // in edge_excluded as absent. Shared by Phase 4's bridge test (exclude the
    // candidate edge plus already-locked ones) and its key-placement pick
    // (exclude the finalized locked set).
    std::vector<bool> ReachableFrom(std::size_t start, std::size_t node_count,
                                    const std::vector<SocketConnection>& connections,
                                    const std::vector<bool>& edge_excluded)
    {
        std::vector<std::vector<std::pair<std::size_t, std::size_t>>> adjacency(node_count);
        for (std::size_t i = 0; i < connections.size(); ++i)
        {
            adjacency[connections[i].piece_a].emplace_back(connections[i].piece_b, i);
            adjacency[connections[i].piece_b].emplace_back(connections[i].piece_a, i);
        }

        std::vector<bool> visited(node_count, false);
        std::queue<std::size_t> queue;
        visited[start] = true;
        queue.push(start);
        while (!queue.empty())
        {
            const std::size_t node = queue.front();
            queue.pop();
            for (const auto& [neighbor, edge_index] : adjacency[node])
            {
                if (edge_excluded[edge_index] || visited[neighbor])
                    continue;
                visited[neighbor] = true;
                queue.push(neighbor);
            }
        }
        return visited;
    }

} // namespace

DungeonLayout GenerateDungeon(const Dungeon& dungeon, const PieceLibrary& library, const SocketLookup& socket_lookup,
                              std::uint64_t seed)
{
    std::mt19937_64 rng{seed};

    // Cache every referenced piece's resolved sockets once -- BuildCandidates
    // is called once per growth attempt and would otherwise re-scan the same
    // piece's cells repeatedly.
    std::unordered_map<std::uint32_t, std::vector<ResolvedSocket>> socket_cache;
    for (const DungeonPieceRef& ref : dungeon.pieces)
        if (const DungeonPiece* piece = library.Find(ref.piece_id))
            if (piece->area_tag == dungeon.area_tag)
                socket_cache.try_emplace(ref.piece_id, ResolveSockets(*piece, socket_lookup));

    const DungeonPieceRef* entrance_ref = nullptr;
    const DungeonPiece* entrance_piece = nullptr;
    for (const DungeonPieceRef& ref : dungeon.pieces)
    {
        const DungeonPiece* piece = library.Find(ref.piece_id);
        if (piece && piece->area_tag == dungeon.area_tag && piece->category == PieceCategory::Entrance)
        {
            entrance_ref = &ref;
            entrance_piece = piece;
            break;
        }
    }
    if (!entrance_piece)
        throw DungeonError("DungeonStitcher: no Entrance piece available in the dungeon's piece pool");
    (void)entrance_ref;

    DungeonLayout layout;
    std::unordered_set<std::int64_t> occupied;
    std::unordered_map<std::uint32_t, int> occurrence_count;
    std::vector<bool> is_tree_edge; // parallel to layout.connections

    auto place_piece = [&](const DungeonPiece& piece, Vec2 world_offset) -> std::size_t
    {
        const std::size_t index = layout.pieces.size();
        layout.pieces.push_back(PlacedPiece{piece.id, world_offset});
        for (const PieceCell& cell : piece.cells)
            occupied.insert(PackCell(world_offset + cell.offset));
        ++occurrence_count[piece.id];
        return index;
    };

    place_piece(*entrance_piece, Vec2{0, 0});

    std::vector<OpenSocket> frontier;
    std::vector<OpenSocket> unconnected;
    for (const ResolvedSocket& socket : socket_cache[entrance_piece->id])
        frontier.push_back(OpenSocket{0, socket.local_cell, socket.edge, socket.info});

    bool exit_placed = false;
    std::size_t exit_index = 0;
    const int target_rooms = std::uniform_int_distribution<int>(dungeon.room_count_min, dungeon.room_count_max)(rng);
    const int max_attempts = std::max(target_rooms, 1) * 50 + 100;
    int attempts = 0;

    while ((static_cast<int>(layout.pieces.size()) < target_rooms || !exit_placed) && !frontier.empty() &&
           attempts < max_attempts)
    {
        ++attempts;
        std::uniform_int_distribution<std::size_t> pick_frontier(0, frontier.size() - 1);
        const std::size_t frontier_index = pick_frontier(rng);
        const OpenSocket open = frontier[frontier_index];
        frontier.erase(frontier.begin() + static_cast<std::ptrdiff_t>(frontier_index));

        std::vector<Candidate> candidates =
            BuildCandidates(dungeon, library, socket_cache, occurrence_count, open, exit_placed);

        bool placed = false;
        while (!candidates.empty() && !placed)
        {
            float total_weight = 0.0f;
            for (const Candidate& candidate : candidates)
                total_weight += std::max(candidate.ref->weight, 0.0f);
            std::size_t chosen = 0;
            if (total_weight <= 0.0f)
                chosen = std::uniform_int_distribution<std::size_t>(0, candidates.size() - 1)(rng);
            else
            {
                std::uniform_real_distribution<float> pick(0.0f, total_weight);
                float roll = pick(rng);
                for (; chosen < candidates.size(); ++chosen)
                {
                    const float weight = std::max(candidates[chosen].ref->weight, 0.0f);
                    if (roll < weight)
                        break;
                    roll -= weight;
                }
                if (chosen >= candidates.size())
                    chosen = candidates.size() - 1;
            }

            const Candidate candidate = candidates[chosen];
            const ResolvedSocket& matching_socket = socket_cache[candidate.piece->id][candidate.socket_index];
            const Vec2 world_offset =
                open.world_cell + EdgeDirectionOffset(open.edge) - matching_socket.local_cell;

            bool overlaps = false;
            for (const PieceCell& cell : candidate.piece->cells)
                if (occupied.contains(PackCell(world_offset + cell.offset)))
                {
                    overlaps = true;
                    break;
                }

            if (overlaps)
            {
                candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(chosen));
                continue;
            }

            const std::size_t new_index = place_piece(*candidate.piece, world_offset);
            layout.connections.push_back(SocketConnection{
                open.piece_index, new_index, open.world_cell, open.world_cell + EdgeDirectionOffset(open.edge)});
            is_tree_edge.push_back(true);

            if (candidate.piece->category == PieceCategory::Exit)
            {
                exit_placed = true;
                exit_index = new_index;
            }

            const std::vector<ResolvedSocket>& all_sockets = socket_cache[candidate.piece->id];
            for (std::size_t i = 0; i < all_sockets.size(); ++i)
            {
                if (i == candidate.socket_index)
                    continue;
                const ResolvedSocket& socket = all_sockets[i];
                frontier.push_back(
                    OpenSocket{new_index, world_offset + socket.local_cell, socket.edge, socket.info});
            }

            placed = true;
        }

        if (!placed)
            unconnected.push_back(open);
    }

    if (!exit_placed)
        throw DungeonError("DungeonStitcher: could not place an Exit piece within the generation attempt budget");

    // Phase 2: loopbacks. Any remaining frontier entries are also unconnected
    // once growth stops (target reached, or attempts/frontier exhausted).
    unconnected.insert(unconnected.end(), frontier.begin(), frontier.end());
    frontier.clear();

    const int target_loopbacks =
        std::uniform_int_distribution<int>(dungeon.loopback_count_min, dungeon.loopback_count_max)(rng);
    int loopbacks_added = 0;
    std::vector<bool> consumed(unconnected.size(), false);
    for (std::size_t i = 0; i < unconnected.size() && loopbacks_added < target_loopbacks; ++i)
    {
        if (consumed[i])
            continue;
        for (std::size_t j = i + 1; j < unconnected.size(); ++j)
        {
            if (consumed[j])
                continue;
            const OpenSocket& a = unconnected[i];
            const OpenSocket& b = unconnected[j];
            if (a.piece_index == b.piece_index)
                continue; // no self-loops
            if (b.edge != OppositeEdge(a.edge))
                continue;
            if (a.world_cell + EdgeDirectionOffset(a.edge) != b.world_cell)
                continue;
            if (!SharesTag(a.info.tags, b.info.tags))
                continue;

            layout.connections.push_back(SocketConnection{a.piece_index, b.piece_index, a.world_cell, b.world_cell});
            is_tree_edge.push_back(false);
            consumed[i] = true;
            consumed[j] = true;
            ++loopbacks_added;
            break;
        }
    }

    // Phase 3: dead ends -- everything still unconsumed after loopbacks.
    for (std::size_t i = 0; i < unconnected.size(); ++i)
        if (!consumed[i])
            layout.dead_ends.push_back(DeadEndSocket{unconnected[i].piece_index, unconnected[i].world_cell,
                                                      unconnected[i].edge, unconnected[i].info.fallback_prefab_id});

    // Phase 4: lock & key. Candidate lock edges are tree edges only -- a
    // loopback edge is never a bridge (both endpoints were already connected
    // through the tree before the loopback was added), so it can never gate
    // entrance-to-exit reachability.
    std::vector<std::size_t> tree_edge_indices;
    for (std::size_t i = 0; i < is_tree_edge.size(); ++i)
        if (is_tree_edge[i])
            tree_edge_indices.push_back(i);

    std::vector<bool> locked(layout.connections.size(), false);
    std::size_t lock_serial = 0;
    for (const DungeonLockConfig& lock_config : dungeon.locks)
    {
        for (int i = 0; i < lock_config.count; ++i)
        {
            std::vector<std::size_t> shuffled = tree_edge_indices;
            std::shuffle(shuffled.begin(), shuffled.end(), rng);

            std::optional<std::size_t> chosen_edge;
            for (std::size_t edge_index : shuffled)
            {
                if (locked[edge_index])
                    continue;
                std::vector<bool> excluded = locked;
                excluded[edge_index] = true;
                std::vector<bool> reachable = ReachableFrom(0, layout.pieces.size(), layout.connections, excluded);
                if (!reachable[exit_index])
                {
                    chosen_edge = edge_index;
                    break;
                }
            }
            if (!chosen_edge)
                continue; // best-effort: no valid bridge left for this lock instance

            locked[*chosen_edge] = true;
            std::vector<bool> reachable_for_key =
                ReachableFrom(0, layout.pieces.size(), layout.connections, locked);
            std::vector<std::size_t> reachable_rooms;
            for (std::size_t room = 0; room < reachable_for_key.size(); ++room)
                if (reachable_for_key[room])
                    reachable_rooms.push_back(room);

            LockAnnotation annotation;
            annotation.edge = layout.connections[*chosen_edge];
            annotation.lock_type = lock_config.lock_type;
            annotation.key_tag = lock_config.lock_type + "_" + std::to_string(lock_serial++);
            annotation.key_room_index =
                reachable_rooms[std::uniform_int_distribution<std::size_t>(0, reachable_rooms.size() - 1)(rng)];
            layout.locks.push_back(std::move(annotation));
        }
    }

    return layout;
}

} // namespace psr
