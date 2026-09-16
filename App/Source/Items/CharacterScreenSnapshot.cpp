#include "Items/CharacterScreenSnapshot.h"

#include "Combat/EffectiveStats.h"
#include "Combat/Element.h"
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
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/ItemComponent.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/RarityComponent.h"
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
            return *label;
        return "Unknown";
    }

    CharacterScreenMessage::ItemEntry BuildItemEntry(const Registry& registry, entt::entity item,
                                                      const AffixLibrary& affixes, const MagComponent* equipped_mag)
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

        if (const RarityComponent* rarity = registry.TryGetComponent<RarityComponent>(item))
            entry.rarity_stars = rarity->stars;
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
        }

        return entry;
    }

    CharacterScreenMessage::MagStatBar BuildMagStatBar(int level, int progress)
    {
        return CharacterScreenMessage::MagStatBar{level, progress, kMagPointsPerLevel};
    }

} // namespace

CharacterScreenMessage BuildCharacterScreenMessage(Registry& registry, entt::entity player,
                                                   const AffixLibrary& affixes, const GrowthCurve& growth_curve)
{
    CharacterScreenMessage message;

    const EquipmentComponent* equipment = registry.TryGetComponent<EquipmentComponent>(player);
    const MagComponent* equipped_mag =
        equipment && equipment->mag != entt::null ? registry.TryGetComponent<MagComponent>(equipment->mag) : nullptr;

    if (const InventoryComponent* inventory = registry.TryGetComponent<InventoryComponent>(player))
    {
        message.inventory.reserve(inventory->items.size());
        for (entt::entity item : inventory->items)
            message.inventory.push_back(BuildItemEntry(registry, item, affixes, equipped_mag));
    }

    if (equipment)
    {
        const std::array<entt::entity, 6> slots = {equipment->weapon, equipment->head,  equipment->torso,
                                                   equipment->hands,  equipment->legs, equipment->mag};
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            if (slots[i] != entt::null)
                message.equipment[i] = BuildItemEntry(registry, slots[i], affixes, equipped_mag);
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
    }

    if (const LevelComponent* level = registry.TryGetComponent<LevelComponent>(player))
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

    const StatsComponent effective_stats = ComputeEffectiveStats(Entity(registry, player), affixes);
    message.stats.atp = effective_stats.atp;
    message.stats.ata = effective_stats.ata;
    message.stats.mst = effective_stats.mst;
    message.stats.dfp = effective_stats.dfp;
    message.stats.evp = effective_stats.evp;
    message.stats.lck = effective_stats.lck;

    return message;
}

} // namespace psr
