#pragma once

#include "Engine/Items/DropTable.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace psr {

// The set of authored Drop Table definitions, loaded from
// App/Assets/Data/DropTables. Mirrors PhotonArtLibrary/AffixLibrary/
// DungeonLibrary.
class DropTableLibrary
{
public:
    DropTableLibrary() = default;
    explicit DropTableLibrary(std::vector<DropTable> drop_tables);

    const DropTable* Find(std::uint32_t id) const;
    const std::vector<DropTable>& All() const { return m_drop_tables; }
    bool Empty() const { return m_drop_tables.empty(); }

private:
    std::vector<DropTable> m_drop_tables;
    std::unordered_map<std::uint32_t, std::size_t> m_by_id;
};

} // namespace psr
