#pragma once

#include "Engine/Dungeon/DungeonPiece.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace psr {

// The set of authored pieces, loaded from App/Assets/Data/Pieces. Owns the
// pieces and indexes them by hashed id for O(1) resolution from a Dungeon
// definition's piece-ref list. Shared by the game (App), the piece editor,
// and DungeonStitcher. Mirrors UnnamedRoguelike's BiomeLibrary.
class PieceLibrary
{
public:
    PieceLibrary() = default;
    explicit PieceLibrary(std::vector<DungeonPiece> pieces);

    // The piece with this hashed id, or nullptr if unknown.
    const DungeonPiece* Find(std::uint32_t id) const;

    // All pieces in load order -- the piece editor's list screen iterates this.
    const std::vector<DungeonPiece>& All() const { return m_pieces; }

    bool Empty() const { return m_pieces.empty(); }

private:
    std::vector<DungeonPiece> m_pieces;
    std::unordered_map<std::uint32_t, std::size_t> m_by_id; // id -> index into m_pieces
};

} // namespace psr
