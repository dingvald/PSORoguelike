#pragma once

#include "Engine/ECS/ComponentSchema.h"

#include <vector>

namespace psr {

// The whole authorable surface of one drop-table file: name, common/rare
// entry lists, guaranteed boss drops, the rare-roll gate, and the Meseta
// range. Mirrors AffixSchemaModel/DungeonSchemaModel's role for their types.
struct DropTableSchemaModel
{
    std::vector<FieldSchema> fields;
};

// Reflects the DropTable data model into a DropTableSchemaModel. Pure and
// self-contained, same shape as BuildAffixSchemaModel/BuildDungeonSchemaModel
// -- hand-built rather than routed through entt::meta reflection, since
// DropTableEntry's nested per-Section-ID weight object isn't a plain leaf
// type EnsureValueTypeRegistered already knows how to map.
DropTableSchemaModel BuildDropTableSchemaModel();

} // namespace psr
