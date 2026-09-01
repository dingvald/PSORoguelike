#include "Engine/Dungeon/DungeonStitcher.h"

#include "Engine/Dungeon/DungeonError.h"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <unordered_map>
#include <unordered_set>

namespace psr {

namespace {

    // A socket on an already-placed piece, not yet connected to anything --
    // a PieceSocket translated into world space plus the piece it came from.
    struct OpenSocket
    {
        std::size_t piece_index;
        Vec2 world_cell;
        EdgeDirection edge;
        std::vector<std::string> tags;
        std::vector<std::string> connects_to_tags;
        std::uint32_t fallback_prefab_id = 0;
    };

    std::int64_t PackCell(Vec2 v) { return (static_cast<std::int64_t>(v.x) << 32) | static_cast<std::uint32_t>(v.y); }

    bool SharesTag(const std::vector<std::string>& a, const std::vector<std::string>& b)
    {
        for (const std::string& tag : a)
            for (const std::string& other : b)
                if (tag == other)
                    return true;
        return false;
    }

    // A connects to B iff either one's connects_to_tags accepts the other's
    // tags -- a one-way filter checked in both directions, not a symmetric
    // tags-to-tags match. tags describes what a socket *is*; connects_to_tags
    // what it *accepts*.
    bool SocketsConnect(const std::vector<std::string>& a_connects_to_tags, const std::vector<std::string>& a_tags,
                        const std::vector<std::string>& b_connects_to_tags, const std::vector<std::string>& b_tags)
    {
        return SharesTag(a_connects_to_tags, b_tags) || SharesTag(b_connects_to_tags, a_tags);
    }

    // One growth candidate: a dungeon-pool piece ref, which of its own
    // sockets would connect to the open socket being grown from, and the
    // orientation it would need to be placed in for that socket to line up.
    struct Candidate
    {
        const DungeonPieceRef* ref;
        const DungeonPiece* piece;
        std::size_t socket_index;
        PieceTransform transform;
    };

    std::vector<Candidate> BuildCandidates(const Dungeon& dungeon, const PieceLibrary& library,
                                           const std::unordered_map<std::uint32_t, int>& occurrence_count,
                                           const OpenSocket& open, bool exit_already_placed,
                                           std::size_t placed_room_count, int target_rooms)
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

            const std::vector<PieceTransform> transforms =
                EnumeratePieceTransforms(piece->can_rotate, piece->can_mirror);
            for (const PieceTransform& transform : transforms)
                for (std::size_t i = 0; i < piece->sockets.size(); ++i)
                {
                    const PieceSocket& candidate_socket = piece->sockets[i];
                    if (ApplyPieceTransform(candidate_socket.edge, transform) == needed_edge &&
                        SocketsConnect(open.connects_to_tags, open.tags, candidate_socket.connects_to_tags,
                                       candidate_socket.tags))
                        candidates.push_back(Candidate{&ref, piece, i, transform});
                }
        }

        // Hold Exit back while the room-count target isn't yet (almost) met and
        // a non-Exit alternative is available -- otherwise Exit, often a
        // single-socket capstone piece, can win an early frontier pick and
        // strand generation with far fewer rooms than room_count_min once its
        // lone socket dead-ends the frontier. If Exit is the only fit for this
        // socket, keep it so generation can still close out rather than fail.
        if (static_cast<int>(placed_room_count) < target_rooms - 1)
        {
            const bool has_non_exit_candidate =
                std::any_of(candidates.begin(), candidates.end(), [](const Candidate& candidate)
                            { return candidate.piece->category != PieceCategory::Exit; });
            if (has_non_exit_candidate)
                candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [](const Candidate& candidate)
                                                { return candidate.piece->category == PieceCategory::Exit; }),
                                 candidates.end());
        }

        return candidates;
    }

    // Candidates for capping a dead-end Corridor socket with a terminal
    // Room/Vault (see Phase 1.5 below) -- same edge/tag matching as growth,
    // but restricted to PieceCategory::Room/Vault, optionally further
    // restricted to pieces whose own DungeonPiece::tags contains "dead_end",
    // and with none of BuildCandidates' Entrance/Exit/room-count bookkeeping,
    // since capping only ever runs after growth has already finished.
    std::vector<Candidate> BuildCapCandidates(const Dungeon& dungeon, const PieceLibrary& library,
                                              const std::unordered_map<std::uint32_t, int>& occurrence_count,
                                              const OpenSocket& open, bool require_dead_end_tag)
    {
        std::vector<Candidate> candidates;
        const EdgeDirection needed_edge = OppositeEdge(open.edge);

        for (const DungeonPieceRef& ref : dungeon.pieces)
        {
            const DungeonPiece* piece = library.Find(ref.piece_id);
            if (!piece || piece->area_tag != dungeon.area_tag)
                continue;
            if (piece->category != PieceCategory::Room && piece->category != PieceCategory::Vault)
                continue;
            if (require_dead_end_tag &&
                std::find(piece->tags.begin(), piece->tags.end(), "dead_end") == piece->tags.end())
                continue;
            if (ref.max_occurrences != 0)
            {
                auto it = occurrence_count.find(ref.piece_id);
                if (it != occurrence_count.end() && it->second >= ref.max_occurrences)
                    continue;
            }

            const std::vector<PieceTransform> transforms =
                EnumeratePieceTransforms(piece->can_rotate, piece->can_mirror);
            for (const PieceTransform& transform : transforms)
                for (std::size_t i = 0; i < piece->sockets.size(); ++i)
                {
                    const PieceSocket& candidate_socket = piece->sockets[i];
                    if (ApplyPieceTransform(candidate_socket.edge, transform) == needed_edge &&
                        SocketsConnect(open.connects_to_tags, open.tags, candidate_socket.connects_to_tags,
                                       candidate_socket.tags))
                        candidates.push_back(Candidate{&ref, piece, i, transform});
                }
        }
        return candidates;
    }

    // Prefers a Room/Vault tagged "dead_end"; falls back to any Room/Vault if
    // none of the tagged ones fit this particular socket (wrong tags/edge,
    // occurrence cap exhausted, or none authored at all).
    std::vector<Candidate>
    BuildCapCandidatesPreferTagged(const Dungeon& dungeon, const PieceLibrary& library,
                                   const std::unordered_map<std::uint32_t, int>& occurrence_count,
                                   const OpenSocket& open)
    {
        std::vector<Candidate> tagged = BuildCapCandidates(dungeon, library, occurrence_count, open, true);
        if (!tagged.empty())
            return tagged;
        return BuildCapCandidates(dungeon, library, occurrence_count, open, false);
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

DungeonLayout GenerateDungeon(const Dungeon& dungeon, const PieceLibrary& library, std::uint64_t seed)
{
    std::mt19937_64 rng{seed};

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

    auto place_piece = [&](const DungeonPiece& piece, Vec2 world_offset, PieceTransform transform) -> std::size_t
    {
        const std::size_t index = layout.pieces.size();
        layout.pieces.push_back(PlacedPiece{piece.id, world_offset, transform});
        for (const PieceCell& cell : piece.cells)
            occupied.insert(PackCell(world_offset + ApplyPieceTransform(cell.offset, transform)));
        ++occurrence_count[piece.id];
        return index;
    };

    // Weighted-random pick among candidates, retrying on overlap until one
    // fits or all are exhausted; records the connection on success. Shared by
    // Phase 1 growth and Phase 1.5 capping so both agree on placement/overlap
    // rules. Returns the new piece's index and which of its sockets was
    // consumed by the connection.
    auto place_best_candidate = [&](std::vector<Candidate> candidates,
                                    const OpenSocket& open) -> std::optional<std::pair<std::size_t, std::size_t>>
    {
        while (!candidates.empty())
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
            const PieceSocket& matching_socket = candidate.piece->sockets[candidate.socket_index];
            const Vec2 world_offset = open.world_cell + EdgeDirectionOffset(open.edge) -
                                      ApplyPieceTransform(matching_socket.cell_offset, candidate.transform);

            bool overlaps = false;
            for (const PieceCell& cell : candidate.piece->cells)
                if (occupied.contains(PackCell(world_offset + ApplyPieceTransform(cell.offset, candidate.transform))))
                {
                    overlaps = true;
                    break;
                }

            if (overlaps)
            {
                candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(chosen));
                continue;
            }

            const std::size_t new_index = place_piece(*candidate.piece, world_offset, candidate.transform);
            layout.connections.push_back(SocketConnection{open.piece_index, new_index, open.world_cell,
                                                          open.world_cell + EdgeDirectionOffset(open.edge)});
            is_tree_edge.push_back(true);
            return std::make_pair(new_index, candidate.socket_index);
        }
        return std::nullopt;
    };

    place_piece(*entrance_piece, Vec2{0, 0}, PieceTransform{});

    std::vector<OpenSocket> frontier;
    std::vector<OpenSocket> unconnected;
    for (const PieceSocket& socket : entrance_piece->sockets)
        frontier.push_back(OpenSocket{0, socket.cell_offset, socket.edge, socket.tags, socket.connects_to_tags,
                                      socket.fallback_prefab_id});

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
            BuildCandidates(dungeon, library, occurrence_count, open, exit_placed, layout.pieces.size(), target_rooms);

        const std::optional<std::pair<std::size_t, std::size_t>> result = place_best_candidate(candidates, open);
        if (result)
        {
            const auto [new_index, matched_socket_index] = *result;
            const DungeonPiece* new_piece = library.Find(layout.pieces[new_index].piece_id);
            const PieceTransform new_transform = layout.pieces[new_index].transform;
            const Vec2 new_offset = layout.pieces[new_index].world_offset;

            if (new_piece->category == PieceCategory::Exit)
            {
                exit_placed = true;
                exit_index = new_index;
            }

            for (std::size_t i = 0; i < new_piece->sockets.size(); ++i)
            {
                if (i == matched_socket_index)
                    continue;
                const PieceSocket& socket = new_piece->sockets[i];
                frontier.push_back(OpenSocket{new_index,
                                              new_offset + ApplyPieceTransform(socket.cell_offset, new_transform),
                                              ApplyPieceTransform(socket.edge, new_transform), socket.tags,
                                              socket.connects_to_tags, socket.fallback_prefab_id});
            }
        }
        else
        {
            unconnected.push_back(open);
        }
    }

    if (!exit_placed)
        throw DungeonError("DungeonStitcher: could not place an Exit piece within the generation attempt budget");

    // Any remaining frontier entries are also unconnected once growth stops
    // (target reached, or attempts/frontier exhausted).
    unconnected.insert(unconnected.end(), frontier.begin(), frontier.end());
    frontier.clear();

    // Phase 1.5: cap dead-end Corridors with Rooms/Vaults, so a hallway that
    // would otherwise terminate in a bare fallback stub (see DeadEndSocket)
    // leads somewhere. Best-effort per socket -- an unmatched dead end just
    // falls through to Phase 3 unchanged. A capped piece is not grown
    // further and takes no part in Phase 2 loopback matching -- its own
    // unused sockets collapse straight to fallback-stamped dead ends here,
    // rather than risking a loopback stitching two capped rooms together
    // behind the player's back.
    std::vector<bool> consumed(unconnected.size(), false);
    for (std::size_t i = 0; i < unconnected.size(); ++i)
    {
        const OpenSocket& open = unconnected[i];
        const DungeonPiece* owner = library.Find(layout.pieces[open.piece_index].piece_id);
        if (!owner || owner->category != PieceCategory::Corridor)
            continue;

        std::vector<Candidate> cap_candidates =
            BuildCapCandidatesPreferTagged(dungeon, library, occurrence_count, open);
        const std::optional<std::pair<std::size_t, std::size_t>> result = place_best_candidate(cap_candidates, open);
        if (!result)
            continue;
        consumed[i] = true;

        const auto [new_index, matched_socket_index] = *result;
        const DungeonPiece* new_piece = library.Find(layout.pieces[new_index].piece_id);
        const PieceTransform new_transform = layout.pieces[new_index].transform;
        const Vec2 new_offset = layout.pieces[new_index].world_offset;
        for (std::size_t s = 0; s < new_piece->sockets.size(); ++s)
        {
            if (s == matched_socket_index)
                continue;
            const PieceSocket& socket = new_piece->sockets[s];
            layout.dead_ends.push_back(
                DeadEndSocket{new_index, new_offset + ApplyPieceTransform(socket.cell_offset, new_transform),
                              ApplyPieceTransform(socket.edge, new_transform), socket.fallback_prefab_id});
        }
    }

    // Phase 2: loopbacks.
    const int target_loopbacks =
        std::uniform_int_distribution<int>(dungeon.loopback_count_min, dungeon.loopback_count_max)(rng);
    int loopbacks_added = 0;
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
            if (!SocketsConnect(a.connects_to_tags, a.tags, b.connects_to_tags, b.tags))
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
                                                     unconnected[i].edge, unconnected[i].fallback_prefab_id});

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
            std::vector<bool> reachable_for_key = ReachableFrom(0, layout.pieces.size(), layout.connections, locked);
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
