#pragma once

#include <entt/entt.hpp>

#include <vector>

namespace psr {

// Item entities an actor has put into hub storage -- runtime state only,
// same "deliberately NOT meta-registered" precedent as InventoryComponent/
// EquipmentComponent. Unlike InventoryComponent, deliberately uncapped (no
// capacity field): storage exists precisely so a run's loot isn't bounded by
// the inventory's carry limit. Populated by Items/Storage.h's StoreItem/
// WithdrawItem; hardcoded-emplaced on the player in
// GameplayLayer::SpawnPlayer, same as InventoryComponent. Deliberately out of
// scope for character-save persistence (Persistence/CharacterSaveFile.h) --
// only the player entity/equipment/inventory and area unlocks are saved.
struct StorageComponent
{
    std::vector<entt::entity> items;
};

} // namespace psr
