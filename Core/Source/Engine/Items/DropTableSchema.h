#pragma once

#include "Engine/ECS/ComponentSchema.h"

#include <vector>

namespace psr {

// The whole authorable surface of one Drop Table file. Mirrors
// PhotonArtSchemaModel/AffixSchemaModel's role for their own content types.
struct DropTableSchemaModel
{
    std::vector<FieldSchema> fields;
};

// Reflects the DropTable data model (DropEntry's own Describe hook for the
// nested common_entries/rare_entries arrays) into a DropTableSchemaModel.
// Pure and self-contained, same shape as BuildPhotonArtSchemaModel.
DropTableSchemaModel BuildDropTableSchemaModel();

} // namespace psr
