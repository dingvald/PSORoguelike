#include "Items/CharacterScreenSnapshot.h"

#include "Combat/EffectiveStats.h"
#include "Combat/Element.h"
#include "Combat/PhotonArt.h"
#include "Combat/PhotonArtLibrary.h"
#include "Combat/StatusEffect.h"
#include "Combat/StatusEffectLibrary.h"
#include "Components/ConsumableComponent.h"
#include "Components/CurrencyComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/LevelComponent.h"
#include "Components/MagComponent.h"
#include "Components/StatsComponent.h"
#include "Components/TPComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/ExtractDisplayString.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/ItemComponent.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/Equip.h"
#include "Items/ItemDisplayName.h"
#include "Items/Mag/MagFeeding.h" // kMagPointsPerLevel
#include "Items/RaceIds.h"
#include "Messages/CharacterScreenMessage.h"
#include "Progression/GrowthCurve.h"

#include <entt/core/hashed_string.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string>

namespace psr {

namespace {

    bool IsMagFood(const Registry& registry, entt::entity item, const MagComponent* equipped_mag)
    {
        if (!equipped_mag)
            return false;
        const PrefabIdComponent* prefab_id = registry.TryGetComponent<PrefabIdComponent>(item);
        if (!prefab_id)
            return false;
        for (const MagFeedResponse& response : equipped_mag->feed_response)
            if (response.item_prefab_id == prefab_id->value)
                return true;
        return false;
    }

    std::string RaceDisplayName(std::uint32_t race_id)
    {
        for (const auto& [id_string, display_name] : kCanonicalRaces)
            if (entt::hashed_string::value(id_string.data()) == race_id)
                return std::string(display_name);
        if (const std::optional<std::string> label = NameIdRegistry::Find(race_id))
            return ExtractDisplayString(*label);
        return "Unknown";
    }

    const char* AffixStatLabel(AffixStat stat)
    {
        switch (stat)
        {
        case AffixStat::Atp:
            return "ATP";
        case AffixStat::Ata:
            return "ATA";
        case AffixStat::Mst:
            return "MST";
        case AffixStat::Dfp:
            return "DFP";
        case AffixStat::Evp:
            return "EVP";
        case AffixStat::Lck:
            return "LCK";
        }
        return "?"; // unreachable for a valid enum value
    }

    int StatValueFor(const StatsComponent& stats, AffixStat stat)
    {
        switch (stat)
        {
        case AffixStat::Atp:
            return stats.atp;
        case AffixStat::Ata:
            return stats.ata;
        case AffixStat::Mst:
            return stats.mst;
        case AffixStat::Dfp:
            return stats.dfp;
        case AffixStat::Evp:
            return stats.evp;
        case AffixStat::Lck:
            return stats.lck;
        }
        return 0; // unreachable for a valid enum value
    }

    // Fills entry.requirement_text/requirement_met from a weapon's
    // stat_requirement or an armor's required_level (whichever the item
    // carries) against the player's current effective stats/level --
    // requirement_met stays true (the default) for an item with neither.
    void ApplyRequirement(CharacterScreenMessage::ItemEntry& entry, const Registry& registry, entt::entity item,
                          const StatsComponent& player_effective_stats, int player_level)
    {
        if (const WeaponComponent* weapon = registry.TryGetComponent<WeaponComponent>(item))
        {
            if (weapon->stat_requirement.value > 0)
            {
                entry.requirement_text = std::string("Requires ") + std::to_string(weapon->stat_requirement.value) +
                                          " " + AffixStatLabel(weapon->stat_requirement.stat);
                entry.requirement_met =
                    StatValueFor(player_effective_stats, weapon->stat_requirement.stat) >= weapon->stat_requirement.value;
            }
            return;
        }

        if (const ArmorComponent* armor = registry.TryGetComponent<ArmorComponent>(item))
        {
            if (armor->required_level > 0)
            {
                entry.requirement_text = "Requires Level " + std::to_string(armor->required_level);
                entry.requirement_met = player_level >= armor->required_level;
            }
        }
    }

    CharacterScreenMessage::ItemEntry BuildItemEntry(const Registry& registry, entt::entity item,
                                                      const AffixLibrary& affixes, const MagComponent* equipped_mag,
                                                      const PhotonArtLibrary& photon_arts,
                                                      const StatusEffectLibrary& status_effects,
                                                      const StatsComponent& player_effective_stats, int player_level)
    {
        CharacterScreenMessage::ItemEntry entry;
        entry.display_name = FormatItemDisplayName(registry, item, affixes);
        entry.equip_slot = ResolveEquipSlot(registry, item);
        entry.is_consumable = registry.HasComponent<ConsumableComponent>(item);
        if (const ItemComponent* item_component = registry.TryGetComponent<ItemComponent>(item))
        {
            entry.quantity = item_component->quantity;
            entry.description = item_component->description;
        }
        if (const ArmorComponent* armor = registry.TryGetComponent<ArmorComponent>(item))
            entry.mod_slot_labels.assign(static_cast<std::size_t>(armor->mod_slot_count), "(empty)");
        entry.is_mag_food = IsMagFood(registry, item, equipped_mag);

        entry.rarity_stars = ResolveDisplayRarity(registry, item);
        ApplyRequirement(entry, registry, item, player_effective_stats, player_level);
        if (const StatsComponent* stats = registry.TryGetComponent<StatsComponent>(item))
            entry.stats = *stats;

        if (const WeaponComponent* weapon = registry.TryGetComponent<WeaponComponent>(item))
        {
            if (weapon->element != Element::None)
            {
                if (!entry.description.empty())
                    entry.description += " ";
                entry.description += ElementDescription(weapon->element);
            }
            for (const RaceBonusEntry& bonus : weapon->race_bonuses)
                entry.species_bonuses.emplace_back(RaceDisplayName(bonus.race_id), bonus.bonus_percent);

            CharacterScreenMessage::ItemEntry::WeaponDetail detail;
            detail.range_shape = weapon->range_shape;
            detail.range = weapon->range;
            detail.hits_per_turn = weapon->hits_per_turn;
            detail.grind_level = weapon->grind_level;
            detail.max_grind_level = weapon->max_grind_level;
            detail.fires_projectile = weapon->fires_projectile;
            detail.targeting_mode = weapon->targeting_mode;
            if (weapon->element != Element::None && weapon->status_chance_percent > 0)
            {
                if (const StatusEffect* effect = status_effects.Find(weapon->status_effect_id))
                {
                    detail.status_effect_name = effect->name;
                    detail.status_chance_percent = weapon->status_chance_percent;
                }
            }
            for (std::uint32_t photon_art_id : weapon->photon_art_ids)
                if (const PhotonArt* art = photon_arts.Find(photon_art_id))
                    detail.photon_art_names.push_back(art->name);
            entry.weapon_detail = std::move(detail);
        }

        return entry;
    }

    CharacterScreenMessage::MagStatBar BuildMagStatBar(int level, int progress)
    {
        return CharacterScreenMessage::MagStatBar{level, progress, kMagPointsPerLevel};
    }

} // namespace

CharacterScreenMessage BuildCharacterScreenMessage(Registry& registry, entt::entity player,
                                                   const AffixLibrary& affixes, const GrowthCurve& growth_curve,
                                                   const PhotonArtLibrary& photon_arts,
                                                   const StatusEffectLibrary& status_effects)
{
    CharacterScreenMessage message;

    const EquipmentComponent* equipment = registry.TryGetComponent<EquipmentComponent>(player);
    const MagComponent* equipped_mag =
        equipment && equipment->mag != entt::null ? registry.TryGetComponent<MagComponent>(equipment->mag) : nullptr;

    // Computed up front (rather than at this function's tail, as message.stats
    // itself is) so BuildItemEntry can check each item's requirement against
    // the player's *current* level/effective stats, not the candidate item's
    // own would-be contribution.
    const LevelComponent* level = registry.TryGetComponent<LevelComponent>(player);
    const int player_level = level ? level->level : 1;
    const StatsComponent player_effective_stats = ComputeEffectiveStats(Entity(registry, player), affixes);

    if (const InventoryComponent* inventory = registry.TryGetComponent<InventoryComponent>(player))
    {
        message.inventory.reserve(inventory->items.size());
        for (entt::entity item : inventory->items)
            message.inventory.push_back(BuildItemEntry(registry, item, affixes, equipped_mag, photon_arts,
                                                        status_effects, player_effective_stats, player_level));
    }

    if (equipment)
    {
        const std::array<entt::entity, 6> slots = {equipment->weapon, equipment->head,  equipment->torso,
                                                   equipment->hands,  equipment->legs, equipment->mag};
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            if (slots[i] != entt::null)
                message.equipment[i] = BuildItemEntry(registry, slots[i], affixes, equipped_mag, photon_arts,
                                                       status_effects, player_effective_stats, player_level);
        }
    }

    if (equipped_mag)
    {
        message.mag = CharacterScreenMessage::MagSummary{};
        message.mag->level = MagLevel(*equipped_mag);
        message.mag->pow = BuildMagStatBar(equipped_mag->pow_level, equipped_mag->pow_progress);
        message.mag->def = BuildMagStatBar(equipped_mag->def_level, equipped_mag->def_progress);
        message.mag->dex = BuildMagStatBar(equipped_mag->dex_level, equipped_mag->dex_progress);
        message.mag->mind = BuildMagStatBar(equipped_mag->mind_level, equipped_mag->mind_progress);
        message.mag->iq = equipped_mag->iq;
        message.mag->sync = equipped_mag->sync;
        message.mag->feed_charges_used = equipped_mag->feed_charges_used;
        message.mag->feed_charges = equipped_mag->feed_charges;
    }

    if (level)
    {
        message.stats.level = level->level;
        message.stats.xp = level->xp;
        message.stats.total_xp = level->total_xp;
        message.stats.xp_to_next = growth_curve.Evaluate(level->level + 1).xp_to_next;
    }

    if (const CurrencyComponent* currency = registry.TryGetComponent<CurrencyComponent>(player))
        message.meseta = currency->meseta;

    if (const HealthComponent* health = registry.TryGetComponent<HealthComponent>(player))
    {
        message.stats.hp = health->current_hp;
        message.stats.max_hp = health->max_hp;
    }

    if (const TPComponent* tp = registry.TryGetComponent<TPComponent>(player))
    {
        message.stats.tp = tp->current_tp;
        message.stats.max_tp = tp->max_tp;
    }

    message.stats.atp = player_effective_stats.atp;
    message.stats.ata = player_effective_stats.ata;
    message.stats.mst = player_effective_stats.mst;
    message.stats.dfp = player_effective_stats.dfp;
    message.stats.evp = player_effective_stats.evp;
    message.stats.lck = player_effective_stats.lck;

    return message;
}

} // namespace psr
