#include "Actions/UseItemAction.h"

#include "Combat/ActionCost.h"
#include "Components/ConsumableComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/TPComponent.h"
#include "Engine/Combat/HealEvent.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Items/ItemUseEvent.h"
#include "Items/TechniqueLearning.h"

#include <algorithm>
#include <cstddef>

namespace psr {

UseItemAction::UseItemAction(int inventory_index) : m_inventory_index(inventory_index) {}

ActionResult UseItemAction::Perform(Entity actor)
{
    InventoryComponent* inventory = actor.TryGet<InventoryComponent>();
    if (!inventory || m_inventory_index < 0 || m_inventory_index >= static_cast<int>(inventory->items.size()))
        return ActionResult(0);

    const entt::entity item = inventory->items[static_cast<std::size_t>(m_inventory_index)];

    Registry& registry = actor.GetRegistry();
    const ConsumableComponent* consumable = registry.TryGetComponent<ConsumableComponent>(item);
    if (!consumable)
        return ActionResult(0);

    switch (consumable->effect)
    {
    case ConsumableEffect::RestoreHp:
    {
        IncomingHealEvent heal{actor, consumable->amount};
        actor.Dispatch(heal);
        break;
    }
    case ConsumableEffect::RestoreTp:
    {
        if (TPComponent* tp = actor.TryGet<TPComponent>())
            tp->current_tp = std::min(tp->max_tp, tp->current_tp + consumable->amount);
        break;
    }
    case ConsumableEffect::TeachTechnique:
        LearnTechnique(actor, consumable->technique_id, consumable->amount);
        break;
    }

    std::uint32_t item_prefab_id = 0;
    if (const PrefabIdComponent* prefab_id = registry.TryGetComponent<PrefabIdComponent>(item))
        item_prefab_id = prefab_id->value;

    inventory->items.erase(inventory->items.begin() + m_inventory_index);
    registry.DestroyEntity(item);

    AfterItemUseEvent event{item_prefab_id};
    actor.Dispatch(event);

    return ActionResult(EffectiveActCost(actor, kUseItemCost));
}

} // namespace psr
