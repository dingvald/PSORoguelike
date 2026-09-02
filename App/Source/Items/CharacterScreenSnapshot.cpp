#include "Items/CharacterScreenSnapshot.h"

#include "Combat/EffectiveStats.h"
#include "Combat/LevelingConfig.h"
#include "Combat/LevelingMath.h"
#include "Components/EquipmentComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/LevelComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Registry.h"
#include "Items/Equip.h"
#include "Items/ItemDisplayName.h"

#include <array>
#include <cstddef>

namespace psr {

namespace {

    void FillStat(CharacterScreenMessage::StatEntry& entry, const std::string& label, int current, int preview)
    {
        entry.label = label;
        entry.current_value = current;
        if (preview != current)
            entry.preview_value = preview;
    }

} // namespace

CharacterScreenMessage BuildCharacterScreenMessage(Registry& registry, entt::entity player,
                                                    const AffixLibrary& affixes, const LevelingConfig& leveling,
                                                    CharacterScreenFocus focus)
{
    CharacterScreenMessage message;
    message.focus = focus;

    if (const InventoryComponent* inventory = registry.TryGetComponent<InventoryComponent>(player))
    {
        message.inventory.reserve(inventory->items.size());
        for (entt::entity item : inventory->items)
            message.inventory.push_back(
                CharacterScreenMessage::ItemEntry{FormatItemDisplayName(registry, item, affixes)});
    }

    if (const EquipmentComponent* equipment = registry.TryGetComponent<EquipmentComponent>(player))
    {
        const std::array<entt::entity, 5> slots = {equipment->weapon, equipment->head, equipment->torso,
                                                    equipment->hands, equipment->legs};
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            if (slots[i] != entt::null)
                message.equipment[i] =
                    CharacterScreenMessage::ItemEntry{FormatItemDisplayName(registry, slots[i], affixes)};
        }
    }

    if (const LevelComponent* level = registry.TryGetComponent<LevelComponent>(player))
    {
        message.level = level->level;
        message.current_exp = level->current_exp;
        message.exp_to_next_level = ExpRequiredForLevel(level->level + 1, leveling);
    }

    Entity actor(registry, player);
    const StatsComponent current_stats = ComputeEffectiveStats(actor, affixes);

    // Only an inventory-focused row that resolves to an equippable slot ever
    // gets a preview -- a keyboard-focused equipment row, or an inventory
    // item that isn't equippable at all, previews nothing.
    std::optional<StatsComponent> preview_stats;
    if (focus.inventory_index)
    {
        const InventoryComponent* inventory = registry.TryGetComponent<InventoryComponent>(player);
        const int index = *focus.inventory_index;
        if (inventory && index >= 0 && index < static_cast<int>(inventory->items.size()))
        {
            const entt::entity item = inventory->items[static_cast<std::size_t>(index)];
            if (const std::optional<EquipmentSlot> slot = ResolveEquipSlot(registry, item))
                preview_stats = ComputeEffectiveStatsWithSlotOverride(actor, affixes, *slot, item);
        }
    }

    const StatsComponent& shown_preview = preview_stats.value_or(current_stats);
    FillStat(message.stats[0], "ATP", current_stats.atp, shown_preview.atp);
    FillStat(message.stats[1], "ATA", current_stats.ata, shown_preview.ata);
    FillStat(message.stats[2], "MST", current_stats.mst, shown_preview.mst);
    FillStat(message.stats[3], "DFP", current_stats.dfp, shown_preview.dfp);
    FillStat(message.stats[4], "EVP", current_stats.evp, shown_preview.evp);
    FillStat(message.stats[5], "LCK", current_stats.lck, shown_preview.lck);

    return message;
}

} // namespace psr
