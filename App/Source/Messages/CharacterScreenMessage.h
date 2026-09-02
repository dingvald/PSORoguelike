#pragma once

#include "Items/Equip.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace psr {

// Which row (if any) the Character screen should render with a "focused"
// highlight, and -- for an inventory row only -- preview a hypothetical
// equip against. Set either by mouse hover (inventory_index only, never
// equipment_slot) or by CharacterScreenState's keyboard cursor (exactly one
// of the two, matching whichever row it currently sits on).
struct CharacterScreenFocus
{
    std::optional<int> inventory_index;
    std::optional<EquipmentSlot> equipment_slot;
};

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

    struct StatEntry
    {
        std::string label; // "ATP", "ATA", "MST", "DFP", "EVP", "LCK"
        int current_value = 0;

        // Set only when CharacterScreenFocus.inventory_index named an
        // equippable item and this stat would change under it -- see
        // BuildCharacterScreenMessage. Never set from an equipment-slot
        // focus (there's nothing hypothetical to preview there).
        std::optional<int> preview_value;
    };

    // Index-aligned with the player's InventoryComponent::items.
    std::vector<ItemEntry> inventory;

    // Indexed by EquipmentSlot (Weapon, Head, Torso, Hands, Legs); nullopt
    // means that slot is empty.
    std::array<std::optional<ItemEntry>, 5> equipment;

    int level = 1;
    int current_exp = 0;
    int exp_to_next_level = 0;
    std::array<StatEntry, 6> stats; // ATP, ATA, MST, DFP, EVP, LCK, in that order

    // Echoes back whatever CharacterScreenFocus BuildCharacterScreenMessage
    // was called with, purely so HudLayer knows which row (if any) to render
    // with the "focused" highlight class.
    CharacterScreenFocus focus;
};

} // namespace psr
