#pragma once

#include "Engine/ECS/ComponentSchema.h"

#include <vector>

namespace psr {

// The whole authorable surface of one Technique file. Mirrors
// PhotonArtSchemaModel's role for PhotonArt.
struct TechniqueSchemaModel
{
    std::vector<FieldSchema> fields;
};

// Reflects the Technique data model (TechniqueTier's own Describe hook for
// the nested tiers array) into a TechniqueSchemaModel. Pure and
// self-contained, same shape as BuildPhotonArtSchemaModel.
TechniqueSchemaModel BuildTechniqueSchemaModel();

} // namespace psr
