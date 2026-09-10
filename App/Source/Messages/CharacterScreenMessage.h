#pragma once

#include "Items/Equip.h"

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

        // ResolveEquipSlot's result for this item -- nullopt for items with
        // neither a WeaponComponent nor an ArmorComponent (not equippable).
        // Lets HudLayer's context menu offer "Equip" only when it applies,
        // and (for the equipment-slot side) find a matching inventory item
        // to jump focus to.
        std::optional<EquipmentSlot> equip_slot;

        // Whether this item carries a ConsumableComponent -- lets HudLayer's
        // context menu offer "Use" only when it applies.
        bool is_consumable = false;

        // ItemComponent::quantity -- 1 for a non-stackable item or an
        // untouched single pickup. HudLayer appends " xN" to display_name
        // when this is greater than 1.
        int quantity = 1;

        // One "(empty)" placeholder per ArmorComponent::mod_slot_count on
        // this item, empty for non-armor items or armor with no mod slots.
        // Display-only for now -- there's no mechanic yet for inserting a
        // mod into one of these slots (no Mod item content or effects are
        // defined), so every entry reads "(empty)" until that lands.
        std::vector<std::string> mod_slot_labels;
    };

    struct StatsSummary
    {
        // Mirrors LevelComponent -- level/xp/total_xp copied as-is, xp_to_next
        // resolved from GrowthCurve::Find(level + 1) so HudLayer never needs
        // its own GrowthCurve reference. 0 means the player is past the
        // authored curve (no further leveling) -- HudLayer renders that case
        // as "MAX" rather than "X / 0".
        int level = 1;
        int xp = 0;
        int xp_to_next = 0;
        int total_xp = 0;

        int hp = 0;
        int max_hp = 0;
        int tp = 0;
        int max_tp = 0;

        // ComputeEffectiveStats' result -- base StatsComponent plus
        // equipped-item/affix bonuses, the same numbers combat actually uses.
        int atp = 0;
        int ata = 0;
        int mst = 0;
        int dfp = 0;
        int evp = 0;
        int lck = 0;
    };

    // Index-aligned with the player's InventoryComponent::items.
    std::vector<ItemEntry> inventory;

    // Indexed by EquipmentSlot (Weapon, Head, Torso, Hands, Legs); nullopt
    // means that slot is empty.
    std::array<std::optional<ItemEntry>, 5> equipment;

    StatsSummary stats;

    // CurrencyComponent::meseta -- rendered as a non-selectable row pinned to
    // the bottom of the Inventory panel (see HudLayer::OnCharacterScreenState),
    // not one of the `inventory` entries above, so it never consumes a slot
    // index or shows up in CharacterScreenRowCount.
    int meseta = 0;
};

} // namespace psr
