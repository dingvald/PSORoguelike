#pragma once

#include "Items/Equip.h"

namespace psr {

// Published by HudLayer when the player clicks an equipment row on the
// Character screen; GameplayLayer subscribes and calls UnequipSlot --
// HudLayer only ever needs to know which slot was clicked.
struct EquipmentSlotActivatedMessage
{
    EquipmentSlot slot = EquipmentSlot::Weapon;
};

} // namespace psr
