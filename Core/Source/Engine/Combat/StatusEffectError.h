#pragma once

#include <stdexcept>
#include <string>

namespace psr {

// Thrown for a structurally valid StatusEffect JSON document whose *content*
// can't be turned into a StatusEffect -- an unknown type name, etc. A
// malformed or unreadable file (or a schema_version mismatch) surfaces as
// JsonFileError from ReadJsonFile instead. Mirrors AffixError/TechniqueError/
// PhotonArtError.
class StatusEffectError : public std::runtime_error
{
public:
    explicit StatusEffectError(const std::string& message) : std::runtime_error(message) {}
};

} // namespace psr
