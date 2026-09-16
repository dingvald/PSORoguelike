#pragma once

#include "Items/Equip.h"

#include <optional>

namespace psr {

// Published by HudLayer whenever the Character screen's hovered/focused
// Equipment-panel row changes -- the equipment-panel counterpart to
// InventoryItemHoverChangedMessage. GameplayLayer responds with
// CharacterScreenStatPreviewMessage, computed via ComputeUnequipStatDelta
// (the delta from removing whatever occupies `slot`) rather than
// ComputeEquipStatDelta. nullopt means "no preview" (nothing hovered/
// focused, or focus isn't on the Equipment panel).
struct EquipmentSlotHoverChangedMessage
{
    std::optional<EquipmentSlot> slot;
};

} // namespace psr
