#pragma once

#include "Items/Equip.h"

namespace psr {

// Published by HudLayer when the player picks "Remove" from an equipment
// slot's Character-screen context menu; GameplayLayer subscribes and calls
// UnequipSlot. The context menu's "Equip" option never reaches this message
// -- it's a HudLayer-local focus jump to a matching Inventory item instead
// (see HudLayer::ChooseMenuOption), since EquipItem only ever equips by
// inventory index, not by target slot.
struct EquipmentSlotActivatedMessage
{
    EquipmentSlot slot = EquipmentSlot::Weapon;
};

} // namespace psr
