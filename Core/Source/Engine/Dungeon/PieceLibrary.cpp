#include "Engine/Dungeon/PieceLibrary.h"

namespace psr {

PieceLibrary::PieceLibrary(std::vector<DungeonPiece> pieces) : m_pieces(std::move(pieces))
{
    for (std::size_t i = 0; i < m_pieces.size(); ++i)
        m_by_id.emplace(m_pieces[i].id, i);
}

const DungeonPiece* PieceLibrary::Find(std::uint32_t id) const
{
    auto it = m_by_id.find(id);
    return it == m_by_id.end() ? nullptr : &m_pieces[it->second];
}

} // namespace psr
