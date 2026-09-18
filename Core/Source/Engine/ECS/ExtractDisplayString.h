#pragma once

#include <string>

namespace psr {

// Turns an entity id (JsonDirectoryLoader's dot-joined path convention, e.g.
// "weapons.saber", "enemies.savage_wolf" -- see NameIdRegistry.h) into a
// player-facing display string: drops everything up to and including the
// last '.', turns '_' into a space, and capitalizes the first letter of each
// word -- "weapons.saber" -> "Saber", "enemies.savage_wolf" -> "Savage Wolf".
std::string ExtractDisplayString(const std::string& entityId);

} // namespace psr
