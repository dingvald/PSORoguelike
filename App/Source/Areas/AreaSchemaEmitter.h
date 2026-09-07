#pragma once

#include "Areas/AreaSchema.h"

#include <rapidjson/document.h>

namespace psr {

// Builds a JSON Schema (draft-04) describing every valid area file (one area
// per file, see AreaLibraryFile.h for the shape) for the given area model.
rapidjson::Document BuildAreaJsonSchema(const AreaSchemaModel& model);

// Validates an already-parsed area-file document against the schema built
// from model. Throws AreaError (with the offending schema keyword and a
// JSON-pointer to the offending value) if the document does not conform.
void ValidateAreaDocument(const rapidjson::Document& document, const AreaSchemaModel& model);

} // namespace psr
