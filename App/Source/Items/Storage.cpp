#include "Items/Storage.h"

#include "Components/InventoryComponent.h"
#include "Components/StorageComponent.h"

#include <cstddef>

namespace psr {

bool StoreItem(Entity actor, int inventory_index)
{
    InventoryComponent* inventory = actor.TryGet<InventoryComponent>();
    if (!inventory || inventory_index < 0 || inventory_index >= static_cast<int>(inventory->items.size()))
        return false;

    const entt::entity item = inventory->items[static_cast<std::size_t>(inventory_index)];
    inventory->items.erase(inventory->items.begin() + inventory_index);

    StorageComponent& storage = actor.GetOrEmplace<StorageComponent>();
    storage.items.push_back(item);
    return true;
}

bool WithdrawItem(Entity actor, int storage_index)
{
    StorageComponent* storage = actor.TryGet<StorageComponent>();
    if (!storage || storage_index < 0 || storage_index >= static_cast<int>(storage->items.size()))
        return false;

    InventoryComponent& inventory = actor.GetOrEmplace<InventoryComponent>();
    if (static_cast<int>(inventory.items.size()) >= inventory.capacity)
        return false;

    const entt::entity item = storage->items[static_cast<std::size_t>(storage_index)];
    storage->items.erase(storage->items.begin() + storage_index);
    inventory.items.push_back(item);
    return true;
}

} // namespace psr
