#pragma once

#include <stdexcept>
#include <string>

namespace psr {

// Thrown for a structurally valid affix JSON document whose *content* can't
// be turned into an Affix -- an unknown kind/stat name, etc. A malformed or
// unreadable file (or a schema_version mismatch) surfaces as JsonFileError
// from ReadJsonFile instead. Mirrors DungeonError.
class AffixError : public std::runtime_error
{
public:
    explicit AffixError(const std::string& message) : std::runtime_error(message) {}
};

} // namespace psr
