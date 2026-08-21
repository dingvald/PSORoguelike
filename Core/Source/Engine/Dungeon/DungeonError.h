#pragma once

#include <stdexcept>
#include <string>

namespace psr {

// Thrown for a structurally valid piece/dungeon JSON document whose *content*
// can't be turned into a DungeonPiece/Dungeon -- an unknown socket edge name,
// an out-of-range reference, etc. A malformed or unreadable file (or a
// schema_version mismatch) surfaces as JsonFileError from ReadJsonFile
// instead. One error type per subsystem, mirroring EntityLoaderError.
class DungeonError : public std::runtime_error
{
public:
    explicit DungeonError(const std::string& message) : std::runtime_error(message) {}
};

} // namespace psr
