#pragma once

#include <entt/entt.hpp>

namespace psr {

class Registry;
class AffixLibrary;
class PhotonArtLibrary;
class StatusEffectLibrary;
struct CharacterScreenMessage;
struct GrowthCurve;

// Resolves player's InventoryComponent/EquipmentComponent into a fully-
// resolved CharacterScreenMessage for HudLayer to render (see
// ItemDisplayName.h for the per-item name formatting). A pure function
// rather than a method on GameplayLayer or CharacterScreenState so both can
// call it without depending on each other -- same "layers/states never
// reference each other" rule CombatLogBridge.h's doc comment already states.
// Takes a non-const Registry& (rather than const, despite only reading)
// because ComputeEffectiveStats needs an Entity, whose constructor requires
// a non-const Registry&. photon_arts/status_effects resolve a weapon's
// photon_art_ids/status_effect_id to display names for ItemEntry::
// WeaponDetail, same "fully resolved" contract the rest of this message
// already follows.
CharacterScreenMessage BuildCharacterScreenMessage(Registry& registry, entt::entity player,
                                                   const AffixLibrary& affixes, const GrowthCurve& growth_curve,
                                                   const PhotonArtLibrary& photon_arts,
                                                   const StatusEffectLibrary& status_effects);

} // namespace psr
