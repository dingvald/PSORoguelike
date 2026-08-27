#pragma once

#include <stdexcept>
#include <string>

namespace psr {

// Thrown for a structurally valid Drop Table JSON document whose *content*
// can't be turned into a DropTable -- an unknown enum name, etc. A malformed
// or unreadable file (or a schema_version mismatch) surfaces as
// JsonFileError from ReadJsonFile instead. Mirrors PhotonArtError/AffixError.
class DropTableError : public std::runtime_error
{
public:
    explicit DropTableError(const std::string& message) : std::runtime_error(message) {}
};

} // namespace psr
