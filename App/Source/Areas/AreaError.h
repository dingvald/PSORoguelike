#pragma once

#include <stdexcept>
#include <string>

namespace psr {

// Thrown for a structurally valid area JSON document whose *content* can't
// be turned into an Area -- an unknown hazard name, etc. A malformed or
// unreadable file (or a schema_version mismatch) surfaces as JsonFileError
// from ReadJsonFile instead. Mirrors AffixError/DungeonError.
class AreaError : public std::runtime_error
{
public:
    explicit AreaError(const std::string& message) : std::runtime_error(message) {}
};

} // namespace psr
