#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace psr {

// Resolved Character-screen contents for HudLayer to render -- display names
// only, not entity handles, same "fully resolved" contract as
// HotbarStateMessage/LootDropMessage so HudLayer never needs a Registry/
// AffixLibrary reference. Published by CharacterScreenState::OnEnter and
// re-published by GameplayLayer after a successful equip/unequip (see
// CharacterScreenSnapshot.h's BuildCharacterScreenMessage, which both call).
struct CharacterScreenMessage
{
    struct ItemEntry
    {
        std::string display_name;
    };

    // Index-aligned with the player's InventoryComponent::items.
    std::vector<ItemEntry> inventory;

    // Indexed by EquipmentSlot (Weapon, Head, Torso, Hands, Legs); nullopt
    // means that slot is empty.
    std::array<std::optional<ItemEntry>, 5> equipment;
};

} // namespace psr
