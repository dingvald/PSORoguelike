#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <string>

namespace psr {

// Marks a prefab as a pickupable item -- the tag PickupAction looks for to
// tell a ground item apart from everything else that can share a tile (floor,
// wall, decoration, an actor). max_stack authors how many of this item can
// share a single inventory/storage slot (1, the default, means "not
// stackable" -- every item behaves exactly as before). quantity is the
// runtime count currently held in *this* slot's entity; it is deliberately
// not meta-registered (no .Data<>() call) so it's never authorable in a
// prefab JSON and always starts at 1 for a freshly loaded/cloned entity --
// only App/Source/Items/Stacking.h's merge logic ever changes it. description
// (kept last, after quantity, so existing two-arg ItemComponent{max_stack,
// quantity} aggregate-inits across the test suite keep compiling unchanged)
// is flavor text for the item-detail UI (see
// CharacterScreenMessage::ItemEntry); empty by default, since it's authored
// content nothing backfills automatically.
struct ItemComponent
{
    int max_stack = 1;
    int quantity = 1;
    std::string description;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<ItemComponent>("item")
            .Data<&ItemComponent::max_stack>("max_stack")
            .Data<&ItemComponent::description>("description");
    }
};

} // namespace psr
