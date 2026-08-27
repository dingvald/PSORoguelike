#pragma once

#include "Engine/Combat/TechniqueSchema.h"

#include <rapidjson/document.h>

namespace psr {

// Builds a JSON Schema (draft-04) describing every valid Technique file (one
// Technique per file, see TechniqueLibraryFile.h for the shape) for the given
// model.
rapidjson::Document BuildTechniqueJsonSchema(const TechniqueSchemaModel& model);

// Validates an already-parsed Technique file document against the schema
// built from model. Throws TechniqueError (with the offending schema keyword
// and a JSON-pointer to the offending value) if the document does not
// conform.
void ValidateTechniqueDocument(const rapidjson::Document& document, const TechniqueSchemaModel& model);

} // namespace psr
