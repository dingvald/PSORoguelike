#pragma once

#include "Engine/ECS/ComponentSchema.h"

#include <vector>

namespace psr {

// The whole authorable surface of one affix file: name/kind/stat/amount.
// Mirrors DungeonSchemaModel's role for Dungeon.
struct AffixSchemaModel
{
    std::vector<FieldSchema> fields;
};

// Reflects the Affix data model into an AffixSchemaModel. Pure and
// self-contained, same shape as BuildPieceSchemaModel/BuildDungeonSchemaModel.
AffixSchemaModel BuildAffixSchemaModel();

} // namespace psr
