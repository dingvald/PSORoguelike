#pragma once

#include "Messages/StorageMessage.h"

#include <entt/entt.hpp>

namespace psr {

class Registry;
class AffixLibrary;

// Resolves player's InventoryComponent/StorageComponent into a
// StorageMessage.
StorageMessage BuildStorageMessage(Registry& registry, entt::entity player, const AffixLibrary& affixes);

} // namespace psr
