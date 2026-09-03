#include "Items/Hotbar.h"

#include "Components/ConsumableComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/KnownTechniquesComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"

#include <algorithm>
#include <cstddef>

namespace psr {

bool AssignItemToHotbarSlot(Entity actor, int inventory_index, int hotbar_slot)
{
    if (hotbar_slot < 0 || hotbar_slot >= static_cast<int>(HotbarComponent::kSlotCount))
        return false;

    InventoryComponent* inventory = actor.TryGet<InventoryComponent>();
    if (!inventory || inventory_index < 0 || inventory_index >= static_cast<int>(inventory->items.size()))
        return false;

    Registry& registry = actor.GetRegistry();
    const entt::entity item = inventory->items[static_cast<std::size_t>(inventory_index)];
    if (!registry.HasComponent<ConsumableComponent>(item))
        return false;
    const PrefabIdComponent* prefab_id = registry.TryGetComponent<PrefabIdComponent>(item);
    if (!prefab_id)
        return false;

    HotbarComponent& hotbar = actor.GetOrEmplace<HotbarComponent>();
    hotbar.slots[static_cast<std::size_t>(hotbar_slot)] = HotbarSlot{HotbarSlotType::Item, prefab_id->value};
    return true;
}

bool AssignAbilityToHotbarSlot(Entity actor, HotbarSlotType type, std::uint32_t id, int hotbar_slot)
{
    if (hotbar_slot < 0 || hotbar_slot >= static_cast<int>(HotbarComponent::kSlotCount))
        return false;

    if (type == HotbarSlotType::Technique)
    {
        const KnownTechniquesComponent* known = actor.TryGet<KnownTechniquesComponent>();
        if (!known)
            return false;
        const bool is_known = std::any_of(known->known.begin(), known->known.end(),
                                          [id](const KnownTechniqueEntry& entry) { return entry.technique_id == id; });
        if (!is_known)
            return false;
    }
    else if (type == HotbarSlotType::PhotonArt)
    {
        const EquipmentComponent* equipment = actor.TryGet<EquipmentComponent>();
        if (!equipment || equipment->weapon == entt::null)
            return false;
        const WeaponComponent* weapon = actor.GetRegistry().TryGetComponent<WeaponComponent>(equipment->weapon);
        if (!weapon)
            return false;
        const bool is_granted =
            std::find(weapon->photon_art_ids.begin(), weapon->photon_art_ids.end(), id) != weapon->photon_art_ids.end();
        if (!is_granted)
            return false;
    }
    else
    {
        return false;
    }

    HotbarComponent& hotbar = actor.GetOrEmplace<HotbarComponent>();
    hotbar.slots[static_cast<std::size_t>(hotbar_slot)] = HotbarSlot{type, id};
    return true;
}

} // namespace psr
