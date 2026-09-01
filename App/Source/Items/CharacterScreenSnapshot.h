#pragma once

#include <entt/entt.hpp>

namespace psr {

class Registry;
class AffixLibrary;
struct CharacterScreenMessage;

// Resolves player's InventoryComponent/EquipmentComponent into a fully-
// resolved CharacterScreenMessage for HudLayer to render (see
// ItemDisplayName.h for the per-item name formatting). A pure function
// rather than a method on GameplayLayer or CharacterScreenState so both can
// call it without depending on each other -- same "layers/states never
// reference each other" rule CombatLogBridge.h's doc comment already states.
CharacterScreenMessage BuildCharacterScreenMessage(const Registry& registry, entt::entity player,
                                                    const AffixLibrary& affixes);

} // namespace psr
