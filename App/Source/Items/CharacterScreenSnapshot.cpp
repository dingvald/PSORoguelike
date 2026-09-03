#include "Items/CharacterScreenSnapshot.h"

#include "Combat/EffectiveStats.h"
#include "Components/ConsumableComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/StatsComponent.h"
#include "Components/TPComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/Equip.h"
#include "Items/ItemDisplayName.h"
#include "Messages/CharacterScreenMessage.h"

#include <array>
#include <cstddef>

namespace psr {

namespace {

    CharacterScreenMessage::ItemEntry BuildItemEntry(const Registry& registry, entt::entity item,
                                                      const AffixLibrary& affixes)
    {
        CharacterScreenMessage::ItemEntry entry;
        entry.display_name = FormatItemDisplayName(registry, item, affixes);
        entry.equip_slot = ResolveEquipSlot(registry, item);
        entry.is_consumable = registry.HasComponent<ConsumableComponent>(item);
        return entry;
    }

} // namespace

CharacterScreenMessage BuildCharacterScreenMessage(Registry& registry, entt::entity player,
                                                   const AffixLibrary& affixes)
{
    CharacterScreenMessage message;

    if (const InventoryComponent* inventory = registry.TryGetComponent<InventoryComponent>(player))
    {
        message.inventory.reserve(inventory->items.size());
        for (entt::entity item : inventory->items)
            message.inventory.push_back(BuildItemEntry(registry, item, affixes));
    }

    if (const EquipmentComponent* equipment = registry.TryGetComponent<EquipmentComponent>(player))
    {
        const std::array<entt::entity, 5> slots = {equipment->weapon, equipment->head, equipment->torso,
                                                   equipment->hands, equipment->legs};
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            if (slots[i] != entt::null)
                message.equipment[i] = BuildItemEntry(registry, slots[i], affixes);
        }
    }

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
