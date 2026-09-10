#include "Items/Storage.h"

#include "Components/InventoryComponent.h"
#include "Components/StorageComponent.h"
#include "Engine/ECS/ItemComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/Stacking.h"

#include <cstddef>

namespace psr {

bool StoreItem(Entity actor, int inventory_index)
{
    InventoryComponent* inventory = actor.TryGet<InventoryComponent>();
    if (!inventory || inventory_index < 0 || inventory_index >= static_cast<int>(inventory->items.size()))
        return false;

    const entt::entity item = inventory->items[static_cast<std::size_t>(inventory_index)];
    inventory->items.erase(inventory->items.begin() + inventory_index);

    Registry& registry = actor.GetRegistry();
    StorageComponent& storage = actor.GetOrEmplace<StorageComponent>();

    // Storage is uncapped, so unlike Inventory it may hold several full
    // stacks of the same item -- top off every existing matching stack with
    // room, then open a new slot for whatever quantity is left over (which
    // is the whole item for non-stackable/unmatched cases, same as before).
    MergeIntoMatchingStacks(registry, item, storage.items, /*stop_after_first_match=*/false);

    const ItemComponent* item_component = registry.TryGetComponent<ItemComponent>(item);
    if (item_component && item_component->quantity == 0)
        registry.DestroyEntity(item);
    else
        storage.items.push_back(item);

    return true;
}

bool WithdrawItem(Entity actor, int storage_index)
{
    StorageComponent* storage = actor.TryGet<StorageComponent>();
    if (!storage || storage_index < 0 || storage_index >= static_cast<int>(storage->items.size()))
        return false;

    const entt::entity item = storage->items[static_cast<std::size_t>(storage_index)];

    Registry& registry = actor.GetRegistry();
    InventoryComponent& inventory = actor.GetOrEmplace<InventoryComponent>();

    const ItemComponent* item_component_before = registry.TryGetComponent<ItemComponent>(item);
    const int quantity_before = item_component_before ? item_component_before->quantity : 0;

    // Inventory keeps its "exactly one slot per stackable type" rule, so a
    // matching stack only ever tops off (possibly partially, leaving the
    // remainder banked) rather than opening a second slot.
    if (MergeIntoMatchingStacks(registry, item, inventory.items, /*stop_after_first_match=*/true))
    {
        const ItemComponent* item_component = registry.TryGetComponent<ItemComponent>(item);
        if (item_component->quantity == 0)
        {
            storage->items.erase(storage->items.begin() + storage_index);
            registry.DestroyEntity(item);
            return true;
        }
        // Partially topped off (some quantity moved, the rest stays banked)
        // or fully blocked (the inventory stack was already at max_stack) --
        // only the former is a real change.
        return item_component->quantity < quantity_before;
    }

    if (static_cast<int>(inventory.items.size()) >= inventory.capacity)
        return false;

    storage->items.erase(storage->items.begin() + storage_index);
    inventory.items.push_back(item);
    return true;
}

} // namespace psr
