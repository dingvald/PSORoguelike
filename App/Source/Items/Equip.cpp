#include "Items/Equip.h"

#include "Components/EquipmentComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/Registry.h"

#include <cstddef>
#include <optional>

namespace psr {

std::optional<EquipmentSlot> ResolveEquipSlot(const Registry& registry, entt::entity item)
{
    if (registry.HasComponent<WeaponComponent>(item))
        return EquipmentSlot::Weapon;

    if (const ArmorComponent* armor = registry.TryGetComponent<ArmorComponent>(item))
    {
        switch (armor->slot)
        {
        case ArmorSlot::Head:
            return EquipmentSlot::Head;
        case ArmorSlot::Torso:
            return EquipmentSlot::Torso;
        case ArmorSlot::Hands:
            return EquipmentSlot::Hands;
        case ArmorSlot::Legs:
            return EquipmentSlot::Legs;
        }
    }

    return std::nullopt;
}

namespace {

    entt::entity& SlotRef(EquipmentComponent& equipment, EquipmentSlot slot)
    {
        switch (slot)
        {
        case EquipmentSlot::Weapon:
            return equipment.weapon;
        case EquipmentSlot::Head:
            return equipment.head;
        case EquipmentSlot::Torso:
            return equipment.torso;
        case EquipmentSlot::Hands:
            return equipment.hands;
        case EquipmentSlot::Legs:
            return equipment.legs;
        }
        return equipment.weapon; // unreachable for a valid enum value
    }

} // namespace

bool EquipItem(Entity actor, int inventory_index)
{
    InventoryComponent* inventory = actor.TryGet<InventoryComponent>();
    if (!inventory || inventory_index < 0 || inventory_index >= static_cast<int>(inventory->items.size()))
        return false;

    Registry& registry = actor.GetRegistry();
    const entt::entity item = inventory->items[static_cast<std::size_t>(inventory_index)];
    const std::optional<EquipmentSlot> slot = ResolveEquipSlot(registry, item);
    if (!slot)
        return false;

    EquipmentComponent& equipment = actor.GetOrEmplace<EquipmentComponent>();
    entt::entity& slot_ref = SlotRef(equipment, *slot);
    const entt::entity previous = slot_ref;

    inventory->items.erase(inventory->items.begin() + inventory_index);
    slot_ref = item;
    if (previous != entt::null)
        inventory->items.push_back(previous);

    return true;
}

bool UnequipSlot(Entity actor, EquipmentSlot slot)
{
    EquipmentComponent* equipment = actor.TryGet<EquipmentComponent>();
    if (!equipment)
        return false;

    entt::entity& slot_ref = SlotRef(*equipment, slot);
    if (slot_ref == entt::null)
        return false;

    InventoryComponent& inventory = actor.GetOrEmplace<InventoryComponent>();
    if (static_cast<int>(inventory.items.size()) >= inventory.capacity)
        return false;

    inventory.items.push_back(slot_ref);
    slot_ref = entt::null;
    return true;
}

} // namespace psr
