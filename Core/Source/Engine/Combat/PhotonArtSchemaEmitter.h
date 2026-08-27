#pragma once

#include "Engine/Combat/PhotonArtSchema.h"

#include <rapidjson/document.h>

namespace psr {

// Builds a JSON Schema (draft-04) describing every valid Photon Art file (one
// Photon Art per file, see PhotonArtLibraryFile.h for the shape) for the
// given model.
rapidjson::Document BuildPhotonArtJsonSchema(const PhotonArtSchemaModel& model);

// Validates an already-parsed Photon Art file document against the schema
// built from model. Throws PhotonArtError (with the offending schema keyword
// and a JSON-pointer to the offending value) if the document does not
// conform.
void ValidatePhotonArtDocument(const rapidjson::Document& document, const PhotonArtSchemaModel& model);

} // namespace psr
