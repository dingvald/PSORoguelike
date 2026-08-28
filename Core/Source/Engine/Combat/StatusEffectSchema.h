#pragma once

#include "Engine/ECS/ComponentSchema.h"

#include <vector>

namespace psr {

// The whole authorable surface of one status-effect file: name/type/
// magnitude/duration. Mirrors AffixSchemaModel's role for Affix.
struct StatusEffectSchemaModel
{
    std::vector<FieldSchema> fields;
};

// Reflects the StatusEffect data model into a StatusEffectSchemaModel. Pure
// and self-contained, same shape as BuildAffixSchemaModel.
StatusEffectSchemaModel BuildStatusEffectSchemaModel();

} // namespace psr
