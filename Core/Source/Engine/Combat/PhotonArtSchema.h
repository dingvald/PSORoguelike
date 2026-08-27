#pragma once

#include "Engine/ECS/ComponentSchema.h"

#include <vector>

namespace psr {

// The whole authorable surface of one Photon Art file. Mirrors
// AffixSchemaModel/DungeonSchemaModel's role for their own content types.
struct PhotonArtSchemaModel
{
    std::vector<FieldSchema> fields;
};

// Reflects the PhotonArt data model (PhotonArtTier's own Describe hook for the
// nested tiers array) into a PhotonArtSchemaModel. Pure and self-contained,
// same shape as BuildDungeonSchemaModel.
PhotonArtSchemaModel BuildPhotonArtSchemaModel();

} // namespace psr
