#pragma once

#include "Engine/Items/DropTableSchema.h"

#include <rapidjson/document.h>

namespace psr {

// Builds a JSON Schema (draft-04) describing every valid Drop Table file (one
// Drop Table per file, see DropTableLibraryFile.h for the shape) for the
// given model.
rapidjson::Document BuildDropTableJsonSchema(const DropTableSchemaModel& model);

// Validates an already-parsed Drop Table file document against the schema
// built from model. Throws DropTableError (with the offending schema keyword
// and a JSON-pointer to the offending value) if the document does not
// conform.
void ValidateDropTableDocument(const rapidjson::Document& document, const DropTableSchemaModel& model);

} // namespace psr
