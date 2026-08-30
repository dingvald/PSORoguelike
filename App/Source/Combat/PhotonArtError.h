#pragma once

#include <stdexcept>
#include <string>

namespace psr {

// Thrown for a structurally valid Photon Art JSON document whose *content*
// can't be turned into a PhotonArt -- an unknown enum name, etc. A malformed
// or unreadable file (or a schema_version mismatch) surfaces as JsonFileError
// from ReadJsonFile instead. Mirrors AffixError/DungeonError.
class PhotonArtError : public std::runtime_error
{
public:
    explicit PhotonArtError(const std::string& message) : std::runtime_error(message) {}
};

} // namespace psr
