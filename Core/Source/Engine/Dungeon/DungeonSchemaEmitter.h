#pragma once

#include "Engine/Dungeon/DungeonSchema.h"

#include <rapidjson/document.h>

namespace psr {

// Builds a JSON Schema (draft-04) describing every valid dungeon file (one
// dungeon per file, see DungeonLibraryFile.h for the shape) for the given
// dungeon model.
rapidjson::Document BuildDungeonJsonSchema(const DungeonSchemaModel& model);

// Validates an already-parsed dungeon-file document against the schema built
// from model. Throws DungeonError (with the offending schema keyword and a
// JSON-pointer to the offending value) if the document does not conform.
void ValidateDungeonDocument(const rapidjson::Document& document, const DungeonSchemaModel& model);

} // namespace psr
