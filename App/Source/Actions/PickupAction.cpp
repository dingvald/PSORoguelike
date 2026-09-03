#include "Actions/PickupAction.h"

#include "Components/BlocksMovementComponent.h"
#include "Components/CurrencyComponent.h"
#include "Components/CurrencyPickupComponent.h"
#include "Components/InventoryComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/ItemComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Items/ItemPickupEvent.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/World/Grid.h"
#include "Messages/MesetaChangedMessage.h"

#include <vector>

namespace psr {

PickupAction::PickupAction(Grid& grid, MessageBus& message_bus) : m_grid(&grid), m_message_bus(&message_bus) {}

ActionResult PickupAction::Perform(Entity actor)
{
    const Vec2 tile = actor.Get<Position>().tile;

    // Copied, not a reference -- RemoveEntity below mutates the very vector
    // GetEntities returns a reference to.
    const std::vector<entt::entity> occupants = m_grid->GetEntities(tile);

    Registry& registry = actor.GetRegistry();

    bool picked_up_any = false;

    for (entt::entity occupant : occupants)
    {
        if (!registry.HasComponent<ItemComponent>(occupant))
            continue;
        const CurrencyPickupComponent* currency_pickup = registry.TryGetComponent<CurrencyPickupComponent>(occupant);
        if (!currency_pickup)
            continue;

        m_grid->RemoveEntity(tile, occupant);
        CurrencyComponent& currency = actor.GetOrEmplace<CurrencyComponent>();
        currency.meseta += currency_pickup->amount;
        m_message_bus->Publish(MesetaChangedMessage{currency.meseta, currency_pickup->amount});
        registry.DestroyEntity(occupant);
        picked_up_any = true;
    }

    for (entt::entity occupant : occupants)
    {
        if (!registry.HasComponent<ItemComponent>(occupant))
            continue;

        InventoryComponent& inventory = actor.GetOrEmplace<InventoryComponent>();
        if (static_cast<int>(inventory.items.size()) >= inventory.capacity)
            break;

        m_grid->RemoveEntity(tile, occupant);
        inventory.items.push_back(occupant);
        picked_up_any = true;

        std::uint32_t item_prefab_id = 0;
        if (const PrefabIdComponent* prefab_id = registry.TryGetComponent<PrefabIdComponent>(occupant))
            item_prefab_id = prefab_id->value;

        AfterItemPickupEvent event{item_prefab_id};
        actor.Dispatch(event);
    }

    return ActionResult(picked_up_any ? kPickupCost : 0);
}

} // namespace psr
