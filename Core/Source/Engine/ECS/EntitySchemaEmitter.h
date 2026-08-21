#pragma once

#include "Engine/ECS/ComponentSchema.h"

#include <rapidjson/document.h>

namespace psr {

// Builds a JSON Schema (draft-04, the dialect RapidJSON's validator speaks)
// describing every valid entity file (see JsonEntityLoader.h for the shape)
// for the given registered component set. The schema enforces the allowed
// component ids, each component's allowed field names, and each field's value
// shape/range. Defaults are not reflected in meta, so no field/axis/channel is
// marked required -- partial objects are valid, matching the loader's
// per-axis/-channel defaults.
rapidjson::Document BuildEntityJsonSchema(const EntitySchemaModel& model);

// Validates an already-parsed entity-file document against the schema built
// from model. Throws EntityLoaderError (with the offending schema keyword and a
// JSON-pointer to the offending value) if the document does not conform.
void ValidateEntityDocument(const rapidjson::Document& document, const EntitySchemaModel& model);

} // namespace psr
