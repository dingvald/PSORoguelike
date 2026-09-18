#pragma once

#include <string>

namespace psr {

// The player's chosen display name (see CharacterCreationLayer's Name step),
// used everywhere DisplayName() would otherwise fall back to the literal
// "Player" -- combat log, target panel, Character screen. Never hand-authored
// in a prefab (deliberately not schema-registered, same "runtime-only"
// precedent as LevelComponent), emplaced once at spawn/restore
// (GameplayLayer).
struct NameComponent
{
    std::string value;
};

} // namespace psr
