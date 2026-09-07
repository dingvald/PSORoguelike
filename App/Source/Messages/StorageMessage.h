#pragma once

#include "Messages/CharacterScreenMessage.h"

#include <vector>

namespace psr {

// Resolved Storage screen contents for HudLayer to render -- reuses
// CharacterScreenMessage::ItemEntry's shape for both lists (same
// display-name/equip-slot/consumable/mod-slot fields the Character screen's
// rows already carry, even though this screen only ever displays the name).
// Published by StorageState::OnEnter and re-published after a successful
// store/withdraw (see Items/StorageSnapshot.h's BuildStorageMessage).
struct StorageMessage
{
    // Index-aligned with the player's InventoryComponent::items.
    std::vector<CharacterScreenMessage::ItemEntry> inventory;

    // Index-aligned with the player's StorageComponent::items.
    std::vector<CharacterScreenMessage::ItemEntry> storage;
};

} // namespace psr
