#include "Items/StorageSnapshot.h"

#include "Components/InventoryComponent.h"
#include "Components/StorageComponent.h"
#include "Engine/ECS/ArmorComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/Equip.h"
#include "Items/ItemDisplayName.h"

#include <cstddef>

namespace psr {

namespace {

    // Duplicates CharacterScreenSnapshot.cpp's own private BuildItemEntry --
    // both are a handful of lines building the same ItemEntry shape from an
    // item entity, and neither call site has any other reason to depend on
    // the other's translation unit, per CLAUDE.md's "three similar lines is
    // better than a premature abstraction."
    CharacterScreenMessage::ItemEntry BuildStorageItemEntry(const Registry& registry, entt::entity item,
                                                            const AffixLibrary& affixes)
    {
        CharacterScreenMessage::ItemEntry entry;
        entry.display_name = FormatItemDisplayName(registry, item, affixes);
        entry.equip_slot = ResolveEquipSlot(registry, item);
        if (const ArmorComponent* armor = registry.TryGetComponent<ArmorComponent>(item))
            entry.mod_slot_labels.assign(static_cast<std::size_t>(armor->mod_slot_count), "(empty)");
        return entry;
    }

} // namespace

StorageMessage BuildStorageMessage(Registry& registry, entt::entity player, const AffixLibrary& affixes)
{
    StorageMessage message;

    if (const InventoryComponent* inventory = registry.TryGetComponent<InventoryComponent>(player))
    {
        message.inventory.reserve(inventory->items.size());
        for (entt::entity item : inventory->items)
            message.inventory.push_back(BuildStorageItemEntry(registry, item, affixes));
    }

    if (const StorageComponent* storage = registry.TryGetComponent<StorageComponent>(player))
    {
        message.storage.reserve(storage->items.size());
        for (entt::entity item : storage->items)
            message.storage.push_back(BuildStorageItemEntry(registry, item, affixes));
    }

    return message;
}

} // namespace psr
