#pragma once

#include "Engine/ECS/ComponentSchema.h"

#include <vector>

namespace psr {

// The whole authorable surface of one area file: name/tag/race/hazard/tile
// palette/unlock predecessor. Mirrors AffixSchemaModel's role for Affix.
struct AreaSchemaModel
{
    std::vector<FieldSchema> fields;
};

// Reflects the Area data model into an AreaSchemaModel. Pure and
// self-contained, same shape as BuildAffixSchemaModel/BuildDungeonSchemaModel.
AreaSchemaModel BuildAreaSchemaModel();

} // namespace psr
