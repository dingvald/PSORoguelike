#pragma once

#include "Messages/CharacterScreenMessage.h"

#include <entt/entt.hpp>

namespace psr {

class Registry;
class AffixLibrary;
struct LevelingConfig;

// Resolves player's InventoryComponent/EquipmentComponent/LevelComponent
// into a fully-resolved CharacterScreenMessage for HudLayer to render (see
// ItemDisplayName.h for the per-item name formatting). A pure function
// rather than a method on GameplayLayer or CharacterScreenState so both can
// call it without depending on each other -- same "layers/states never
// reference each other" rule CombatLogBridge.h's doc comment already states.
//
// registry is non-const (not const Registry&, unlike before) because
// ComputeEffectiveStats/ComputeEffectiveStatsWithSlotOverride need a
// mutable Entity to construct from it. focus, when it names an inventory
// item that resolves to an equippable slot (see Items/Equip.h's
// ResolveEquipSlot), fills each StatEntry's preview_value with what that
// stat would be under the hypothetical swap; otherwise every preview_value
// stays unset. focus is echoed back verbatim on the returned message's own
// `focus` field either way, so HudLayer always knows which row to highlight.
CharacterScreenMessage BuildCharacterScreenMessage(Registry& registry, entt::entity player,
                                                    const AffixLibrary& affixes, const LevelingConfig& leveling,
                                                    CharacterScreenFocus focus = {});

} // namespace psr
