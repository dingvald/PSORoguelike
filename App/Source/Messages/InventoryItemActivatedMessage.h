#pragma once

namespace psr {

// Which action the player chose from an inventory item's Character-screen
// context menu (see HudLayer's BuildMenuOptions/ChooseMenuOption).
enum class InventoryItemAction
{
    Equip,
    Use,
    Drop
};

// Published by HudLayer when the player picks an action from an inventory
// item's context menu on the Character screen; GameplayLayer subscribes and
// routes to EquipItem (free/instant) or a UseItemAction/DropAction submitted
// via TurnCoordinator (see GameplayLayer::OnInventoryItemActivated).
struct InventoryItemActivatedMessage
{
    int inventory_index = -1;
    InventoryItemAction action = InventoryItemAction::Equip;
};

} // namespace psr
