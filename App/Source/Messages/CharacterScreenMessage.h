#pragma once

#include "Components/StatsComponent.h"
#include "Components/WeaponComponent.h" // WeaponRangeShape, reused verbatim
#include "Engine/Combat/TargetingMode.h"
#include "Items/Equip.h"

#include <array>
#include <optional>
#include <string>
#include <utility>
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

        // Whether the player's currently-equipped mag's own
        // MagComponent::feed_response recognizes this item's prefab --
        // lets HudLayer's "select a food to feed the mag" flow (entered from
        // the Feed context-menu action) tell which Inventory rows are valid
        // to select. False when no mag is equipped, or this item isn't
        // equippable-mag food.
        bool is_mag_food = false;

        // RarityComponent::stars, ItemComponent::description (an elemental
        // weapon has ItemDisplayName.h's ElementDescription appended), and
        // this item's own (post-drop-roll) StatsComponent -- all for the
        // Character screen's item-detail panel (see HudLayer::
        // RenderItemDetailPanel). stats is nullopt for an item with no
        // StatsComponent at all (most consumables).
        int rarity_stars = 0;
        std::string description;
        std::optional<StatsComponent> stats;

        // A weapon's StatRequirement or an armor's required_level, formatted
        // for display (e.g. "Requires 100 ATP" / "Requires Level 5") --
        // empty when the item has no requirement. requirement_met compares
        // it against the player's own current effective stats/level, so
        // HudLayer can grey out an inventory/equipment row the player
        // doesn't (yet) qualify for, while still leaving it selectable.
        std::string requirement_text;
        bool requirement_met = true;

        // A weapon's rolled WeaponComponent::race_bonuses, resolved to a
        // display race name (e.g. "Native") + bonus_percent -- fully
        // resolved so HudLayer never needs a race_id -> label lookup of its
        // own. Empty for a non-weapon item or a weapon with no race bonus.
        std::vector<std::pair<std::string, int>> species_bonuses;

        // A weapon's own combat-relevant fields for the item-detail panel,
        // fully resolved (status effect id -> name via StatusEffectLibrary,
        // photon_art_ids -> names via PhotonArtLibrary) so HudLayer never
        // needs either library itself. nullopt for a non-weapon item.
        struct WeaponDetail
        {
            WeaponRangeShape range_shape = WeaponRangeShape::SingleTarget;
            int range = 1;
            int hits_per_turn = 1;
            int grind_level = 0;
            int max_grind_level = 0;
            bool fires_projectile = false;
            TargetingMode targeting_mode = TargetingMode::Directional;

            // Only set when the weapon is elemental and status_chance_percent
            // > 0 -- see WeaponComponent::status_effect_id's own doc comment
            // on why a non-elemental weapon never applies its status_effect_id.
            std::string status_effect_name;
            int status_chance_percent = 0;

            std::vector<std::string> photon_art_names;
        };
        std::optional<WeaponDetail> weapon_detail;
    };

    // One PSO-style stat's mag progress -- level plus progress toward the
    // next level (out of progress_to_next, mirroring MagFeeding.h's
    // kMagPointsPerLevel so HudLayer never needs that constant itself).
    struct MagStatBar
    {
        int level = 0;
        int progress = 0;
        int progress_to_next = 1;
    };

    // The equipped mag's stats/level/iq/sync for the Character screen's mag
    // panel (see docs and MagComponent.h) -- nullopt when no mag is
    // equipped, in which case HudLayer hides the panel.
    struct MagSummary
    {
        int level = 0;
        MagStatBar pow;
        MagStatBar def;
        MagStatBar dex;
        MagStatBar mind;
        int iq = 0;
        float sync = 0.0f;

        // MagComponent::feed_charges_used/feed_charges, copied as-is -- lets
        // HudLayer's context menu grey out "Feed" once charges_used reaches
        // charges (feeding's actual gate -- see GameplayLayer::
        // OnMagFeedRequested) instead of offering an action it would just
        // silently reject.
        int feed_charges_used = 0;
        int feed_charges = 3;
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

    // Indexed by EquipmentSlot (Weapon, Head, Torso, Hands, Legs, Mag);
    // nullopt means that slot is empty.
    std::array<std::optional<ItemEntry>, 6> equipment;

    StatsSummary stats;

    // The equipped mag's own panel data -- nullopt when EquipmentSlot::Mag
    // is empty.
    std::optional<MagSummary> mag;

    // CurrencyComponent::meseta -- rendered as a non-selectable row pinned to
    // the bottom of the Inventory panel (see HudLayer::OnCharacterScreenState),
    // not one of the `inventory` entries above, so it never consumes a slot
    // index or shows up in CharacterScreenRowCount.
    int meseta = 0;

    // Set only by GameplayLayer::OnMagFeedRequested's republish after a
    // successful feed (never by CharacterScreenState::OnEnter or any other
    // republish path). MessageBus::Publish only enqueues -- handler
    // invocation is deferred to the subscriber's own HandleQueuedMessages,
    // so HudLayer can't tell "this refresh followed a feed" apart from any
    // other refresh via a flag it flips around the Publish call (that flag
    // is long reset by the time this message is actually handled). Carrying
    // the fact on the message itself sidesteps that timing entirely -- lets
    // HudLayer::OnCharacterScreenState chain straight back into another
    // food selection (or land on the Mag slot's own context menu, Feed
    // disabled, once out of charges) instead of stranding focus.
    bool fed_mag = false;
};

} // namespace psr
