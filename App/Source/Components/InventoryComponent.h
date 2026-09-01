#pragma once

#include <entt/entt.hpp>

#include <vector>

namespace psr {

// Item entities an actor is carrying (removed from the Grid while held) --
// runtime state only, same "deliberately NOT meta-registered" precedent as
// EquipmentComponent (entt::entity/std::vector<entt::entity> have no
// FieldKind mapping, so neither this component nor its contents can be
// hand-authored in a prefab JSON). Populated by PickupAction/DropAction;
// hardcoded-emplaced on the player in GameplayLayer::LoadNewGame, same as
// SectionIdComponent/CurrencyComponent, since there's no character creation
// yet to size it differently per class.
struct InventoryComponent
{
    static constexpr int kDefaultCapacity = 20;

    std::vector<entt::entity> items;
    int capacity = kDefaultCapacity;
};

} // namespace psr
