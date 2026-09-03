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
// Takes a non-const Registry& (rather than const, despite only reading)
// because ComputeEffectiveStats needs an Entity, whose constructor requires
// a non-const Registry&.
CharacterScreenMessage BuildCharacterScreenMessage(Registry& registry, entt::entity player,
                                                   const AffixLibrary& affixes);

} // namespace psr
