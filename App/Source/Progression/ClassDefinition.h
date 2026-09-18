#pragma once

#include "Progression/CharacterClass.h"
#include "Progression/GrowthCurve.h"

#include <string>
#include <vector>

namespace psr {

// One entry of a class's starting inventory -- an item prefab plus how many
// of it to spawn into that one slot's ItemComponent::quantity (see
// GameplayLayer::SpawnNewCharacter). Not a pickup: created directly into
// InventoryComponent, so Stacking.h's merge path never runs on it.
struct StartingInventoryEntry
{
    std::string item_prefab_id;
    int quantity = 1;
};

// One class's fixed starting kit plus its own level-up curve -- replaces the
// old single class-agnostic growth_curve.json now that ClassId exists (see
// GameplayLayer::SpawnNewCharacter). Loaded once, for whichever class the
// player chose at character creation, via ClassDefinitionFile.h.
struct ClassDefinition
{
    ClassId class_id = ClassId::Hunter;
    std::string name;

    // Ids only, resolved via entt::hashed_string at the point of use -- same
    // "no NameIdRegistry-resolved _string companion field" convention every
    // other plain-tag content reference in this project already follows
    // (see M4.1's own note in docs/ROADMAP.md).
    std::string starting_weapon_prefab_id;
    std::vector<std::string> starting_technique_id_strings; // empty for Hunter/Ranger
    std::string starting_armor_prefab_id;                   // empty = nothing equipped
    std::string starting_mag_prefab_id;                     // empty = nothing equipped
    std::vector<StartingInventoryEntry> starting_inventory;

    int base_hp = 0;
    int base_tp = 0;

    GrowthCurve growth;
};

} // namespace psr
