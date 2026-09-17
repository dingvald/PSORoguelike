#include "Engine/Dungeon/DungeonStitcher.h"

#include "Engine/Dungeon/DungeonError.h"

#include <algorithm>
#include <cstdint>
#include <limits>
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

    // The EdgeDirection a piece must present at `from` for its socket to
    // border an adjacent cell at `to` -- used to recover which of a
    // surviving piece's own sockets a retracted dead-end Corridor chain (see
    // Phase 1.5b below) used to connect through, since SocketConnection
    // itself only stores the two world cells, not the direction between
    // them.
    EdgeDirection EdgeDirectionBetween(Vec2 from, Vec2 to)
    {
        const Vec2 diff = to - from;
        for (EdgeDirection candidate :
             {EdgeDirection::North, EdgeDirection::East, EdgeDirection::South, EdgeDirection::West})
            if (EdgeDirectionOffset(candidate) == diff)
                return candidate;
        return EdgeDirection::North;
    }

    // Looks up a placed piece's own fallback_prefab_id for whichever of its
    // authored sockets sits at world_cell/edge, mirroring
    // DungeonInstantiator.cpp's FindDeadEnd but against the piece definition
    // itself rather than an already-recorded DeadEndSocket.
    std::uint32_t FindSocketFallback(const DungeonPiece* piece, Vec2 world_offset, PieceTransform transform,
                                     Vec2 world_cell, EdgeDirection edge)
    {
        if (!piece)
            return 0;
        for (const PieceSocket& socket : piece->sockets)
            if (world_offset + ApplyPieceTransform(socket.cell_offset, transform) == world_cell &&
                ApplyPieceTransform(socket.edge, transform) == edge)
                return socket.fallback_prefab_id;
        return 0;
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
    layout.unlocked_door_prefab_id = dungeon.unlocked_door_prefab_id;
    layout.locked_door_prefab_id = dungeon.locked_door_prefab_id;
    layout.switch_prefab_id = dungeon.switch_prefab_id;
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

    // parent_edge_of/child_count describe the growth tree as it stood the
    // moment Phase 1 stopped: parent_edge_of[p] is the index into
    // layout.connections of the one tree edge that placed p (every piece but
    // the Entrance has exactly one), child_count[p] how many further pieces
    // grew from p's own other sockets. Phase 1.5b (below) mutates both as it
    // retracts dead branches; a capping placement in 1.5a also bumps
    // child_count for the Corridor it caps, since that Corridor now leads
    // somewhere and must never be retracted.
    std::unordered_map<std::size_t, std::size_t> parent_edge_of;
    std::unordered_map<std::size_t, int> child_count;
    for (std::size_t e = 0; e < layout.connections.size(); ++e)
    {
        ++child_count[layout.connections[e].piece_a];
        parent_edge_of[layout.connections[e].piece_b] = e;
    }

    // Phase 1.5a: cap dead-end Corridors with Rooms/Vaults, so a hallway that
    // would otherwise terminate in a bare fallback stub (see DeadEndSocket)
    // leads somewhere. Best-effort per socket -- an unmatched dead end is
    // handled by 1.5b below instead of falling straight through to Phase 3.
    // A capped piece is not grown further and takes no part in Phase 2
    // loopback matching -- its own unused sockets collapse straight to
    // fallback-stamped dead ends here, rather than risking a loopback
    // stitching two capped rooms together behind the player's back.
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
        ++child_count[open.piece_index]; // now leads to the capping piece -- never a retraction target

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

    // Phase 1.5b: a Corridor that still has zero live children after 1.5a
    // leads nowhere at all -- rather than leave it dead-ending in a bare
    // fallback stub, remove it outright, cascading the same check up through
    // its parent (removing it just zeroed the parent's own child count too,
    // so the parent may now qualify as well), and so on, until the chain
    // reaches either the Entrance or a piece with at least one other live
    // child. That stopping piece's own now-exposed socket is what actually
    // dead-ends -- fallback-stamped exactly like an ordinary Phase 3 dead
    // end, and likewise never fed back into Phase 2 loopback matching.
    std::unordered_set<std::size_t> removed;
    std::vector<bool> edge_removed(layout.connections.size(), false);

    std::vector<std::size_t> retraction_worklist;
    for (std::size_t p = 1; p < layout.pieces.size(); ++p) // 0 is the Entrance, never removable
    {
        const DungeonPiece* piece = library.Find(layout.pieces[p].piece_id);
        if (piece && piece->category == PieceCategory::Corridor && child_count[p] == 0)
            retraction_worklist.push_back(p);
    }
    while (!retraction_worklist.empty())
    {
        const std::size_t current = retraction_worklist.back();
        retraction_worklist.pop_back();
        if (removed.contains(current))
            continue;
        removed.insert(current);
        --occurrence_count[layout.pieces[current].piece_id];

        const std::size_t edge_index = parent_edge_of.at(current);
        edge_removed[edge_index] = true;
        const std::size_t parent = layout.connections[edge_index].piece_a;
        --child_count[parent];

        const DungeonPiece* parent_piece = library.Find(layout.pieces[parent].piece_id);
        if (parent != 0 && parent_piece && parent_piece->category == PieceCategory::Corridor &&
            child_count[parent] == 0)
            retraction_worklist.push_back(parent);
    }

    // Every cut edge whose surviving (parent) side wasn't itself removed is a
    // freshly exposed dead end on that survivor.
    for (std::size_t e = 0; e < edge_removed.size(); ++e)
    {
        if (!edge_removed[e] || removed.contains(layout.connections[e].piece_a))
            continue;
        const SocketConnection& cut_edge = layout.connections[e];
        const std::size_t stop_index = cut_edge.piece_a;
        const EdgeDirection freed_edge = EdgeDirectionBetween(cut_edge.cell_a, cut_edge.cell_b);
        const std::uint32_t fallback_prefab_id =
            FindSocketFallback(library.Find(layout.pieces[stop_index].piece_id), layout.pieces[stop_index].world_offset,
                               layout.pieces[stop_index].transform, cut_edge.cell_a, freed_edge);
        layout.dead_ends.push_back(DeadEndSocket{stop_index, cut_edge.cell_a, freed_edge, fallback_prefab_id});
    }

    for (std::size_t i = 0; i < unconnected.size(); ++i)
        if (removed.contains(unconnected[i].piece_index))
            consumed[i] = true;

    // Compact away every retracted piece/edge before Phase 2/3/4 run, so
    // every later index refers only to a piece that actually still exists.
    if (!removed.empty())
    {
        std::vector<std::size_t> index_remap(layout.pieces.size(), std::numeric_limits<std::size_t>::max());
        std::vector<PlacedPiece> new_pieces;
        for (std::size_t old_index = 0; old_index < layout.pieces.size(); ++old_index)
        {
            if (removed.contains(old_index))
                continue;
            index_remap[old_index] = new_pieces.size();
            new_pieces.push_back(layout.pieces[old_index]);
        }
        layout.pieces = std::move(new_pieces);

        std::vector<SocketConnection> new_connections;
        std::vector<bool> new_is_tree_edge;
        for (std::size_t e = 0; e < layout.connections.size(); ++e)
        {
            if (edge_removed[e])
                continue;
            SocketConnection connection = layout.connections[e];
            connection.piece_a = index_remap[connection.piece_a];
            connection.piece_b = index_remap[connection.piece_b];
            new_connections.push_back(connection);
            new_is_tree_edge.push_back(is_tree_edge[e]);
        }
        layout.connections = std::move(new_connections);
        is_tree_edge = std::move(new_is_tree_edge);

        for (DeadEndSocket& dead_end : layout.dead_ends)
            dead_end.piece_index = index_remap[dead_end.piece_index];

        for (OpenSocket& open : unconnected)
            if (index_remap[open.piece_index] != std::numeric_limits<std::size_t>::max())
                open.piece_index = index_remap[open.piece_index];

        exit_index = index_remap[exit_index];
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

    // Phase 4: locks. Candidate lock edges are tree edges only -- a loopback
    // edge is never a bridge (both endpoints were already connected through
    // the tree before the loopback was added), so it can never gate
    // entrance-to-exit reachability. A candidate edge is further only valid
    // if the side it would gate (unreachable once the edge is excluded) is a
    // Room/Vault/BossArena piece -- only those get a physical door (see
    // DungeonInstantiator), and only those author a meaningful
    // preferred_unlock_condition.
    std::vector<std::size_t> tree_edge_indices;
    for (std::size_t i = 0; i < is_tree_edge.size(); ++i)
        if (is_tree_edge[i])
            tree_edge_indices.push_back(i);

    std::vector<bool> locked(layout.connections.size(), false);
    for (int i = 0; i < dungeon.lock_count; ++i)
    {
        std::vector<std::size_t> shuffled = tree_edge_indices;
        std::shuffle(shuffled.begin(), shuffled.end(), rng);

        std::optional<std::size_t> chosen_edge;
        std::size_t inside_index = 0;
        const DungeonPiece* inside_piece = nullptr;
        for (std::size_t edge_index : shuffled)
        {
            if (locked[edge_index])
                continue;
            std::vector<bool> excluded = locked;
            excluded[edge_index] = true;
            std::vector<bool> reachable = ReachableFrom(0, layout.pieces.size(), layout.connections, excluded);
            if (reachable[exit_index])
                continue;

            const SocketConnection& candidate_edge = layout.connections[edge_index];
            const std::size_t candidate_inside =
                reachable[candidate_edge.piece_a] ? candidate_edge.piece_b : candidate_edge.piece_a;
            const DungeonPiece* candidate_piece = library.Find(layout.pieces[candidate_inside].piece_id);
            if (!candidate_piece || (candidate_piece->category != PieceCategory::Room &&
                                     candidate_piece->category != PieceCategory::Vault &&
                                     candidate_piece->category != PieceCategory::BossArena))
                continue;
            // RoomCleared is no longer a curated lock outcome -- pre-locking a door at
            // generation time, before the player can ever enter to start the kill count
            // that would unlock it, is an unenterable dead end. It's now the automatic
            // (default) state for any non-Corridor piece with spawns instead (see
            // DungeonInstantiator::room_entry_doors / RoomClearDoorSystem::LockRoomOnEntry).
            // Only a piece that explicitly opts into Switch is still eligible here.
            if (candidate_piece->preferred_unlock_condition != DoorUnlockCondition::Switch)
                continue;

            chosen_edge = edge_index;
            inside_index = candidate_inside;
            inside_piece = candidate_piece;
            break;
        }
        if (!chosen_edge)
            continue; // best-effort: no valid bridge left for this lock instance

        locked[*chosen_edge] = true;

        LockAnnotation annotation;
        annotation.edge = layout.connections[*chosen_edge];
        annotation.inside_room_index = inside_index;
        annotation.unlock_condition = inside_piece->preferred_unlock_condition;

        if (annotation.unlock_condition == DoorUnlockCondition::Switch)
        {
            std::vector<bool> reachable_for_switch =
                ReachableFrom(0, layout.pieces.size(), layout.connections, locked);
            std::vector<std::size_t> reachable_rooms;
            for (std::size_t room = 0; room < reachable_for_switch.size(); ++room)
                if (reachable_for_switch[room])
                    reachable_rooms.push_back(room);

            annotation.switch_room_index =
                reachable_rooms[std::uniform_int_distribution<std::size_t>(0, reachable_rooms.size() - 1)(rng)];

            const PlacedPiece& switch_placed = layout.pieces[annotation.switch_room_index];
            const DungeonPiece* switch_piece = library.Find(switch_placed.piece_id);
            if (switch_piece && !switch_piece->cells.empty())
            {
                const std::size_t cell_index =
                    std::uniform_int_distribution<std::size_t>(0, switch_piece->cells.size() - 1)(rng);
                annotation.switch_cell = switch_placed.world_offset +
                    ApplyPieceTransform(switch_piece->cells[cell_index].offset, switch_placed.transform);
            }
            else
            {
                annotation.switch_cell = switch_placed.world_offset;
            }
        }

        layout.locks.push_back(std::move(annotation));
    }

    return layout;
}

} // namespace psr
