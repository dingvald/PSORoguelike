#pragma once

#include "Items/AffixSchema.h"

#include <rapidjson/document.h>

namespace psr {

// Builds a JSON Schema (draft-04) describing every valid affix file (one
// affix per file, see AffixLibraryFile.h for the shape) for the given affix
// model.
rapidjson::Document BuildAffixJsonSchema(const AffixSchemaModel& model);

// Validates an already-parsed affix-file document against the schema built
// from model. Throws AffixError (with the offending schema keyword and a
// JSON-pointer to the offending value) if the document does not conform.
void ValidateAffixDocument(const rapidjson::Document& document, const AffixSchemaModel& model);

} // namespace psr
