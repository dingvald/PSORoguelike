#pragma once

#include "Engine/Combat/StatusEffectSchema.h"

#include <rapidjson/document.h>

namespace psr {

// Builds a JSON Schema (draft-04) describing every valid status-effect file
// (one status effect per file, see StatusEffectLibraryFile.h for the shape)
// for the given status-effect model.
rapidjson::Document BuildStatusEffectJsonSchema(const StatusEffectSchemaModel& model);

// Validates an already-parsed status-effect-file document against the schema
// built from model. Throws StatusEffectError (with the offending schema
// keyword and a JSON-pointer to the offending value) if the document does
// not conform.
void ValidateStatusEffectDocument(const rapidjson::Document& document, const StatusEffectSchemaModel& model);

} // namespace psr
