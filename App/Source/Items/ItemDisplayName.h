#pragma once

#include "Combat/Element.h"

#include <entt/entt.hpp>

#include <string>

namespace psr {

class Registry;
class AffixLibrary;

// Resolves item's display name via the same PrefabIdComponent ->
// NameIdRegistry::Find pattern CombatLogBridge::DisplayName/LootDropSystem
// already use, then -- only for a WeaponComponent-tagged item -- decorates it
// with prefix_affix_id's name (prepended), element (prepended, e.g. "Fire"),
// the base name, suffix_affix_id's name (appended as " of <name>"), and
// grind_level (appended as " +N" when nonzero), e.g. "Fire saber of power
// +4". Armor/mod items (no WeaponComponent) just return the base name --
// M8.1 made affixes/grind weapon-only, they have no such fields.
std::string FormatItemDisplayName(const Registry& registry, entt::entity item, const AffixLibrary& affixes);

// A short sentence describing what a rolled elemental "prefix" (see
// EquipmentDropRoller.h) does, for the item-detail panel -- appended to an
// elemental weapon's authored description. Empty for Element::None.
const char* ElementDescription(Element element);

} // namespace psr
