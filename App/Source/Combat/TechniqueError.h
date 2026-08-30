#pragma once

#include <stdexcept>
#include <string>

namespace psr {

// Thrown for a structurally valid Technique JSON document whose *content*
// can't be turned into a Technique -- an unknown enum name, etc. A malformed
// or unreadable file (or a schema_version mismatch) surfaces as JsonFileError
// from ReadJsonFile instead. Mirrors PhotonArtError/AffixError/DungeonError.
class TechniqueError : public std::runtime_error
{
public:
    explicit TechniqueError(const std::string& message) : std::runtime_error(message) {}
};

} // namespace psr
