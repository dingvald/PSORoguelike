#pragma once

#include "Engine/Dungeon/PieceSchema.h"

#include <rapidjson/document.h>

namespace psr {

// Builds a JSON Schema (draft-04) describing every valid piece file (one
// piece per file, see PieceLibraryFile.h for the shape) for the given piece
// model. Pins schema_version and validates name/area_tag/category plus the
// cells array.
rapidjson::Document BuildPieceJsonSchema(const PieceSchemaModel& model);

// Validates an already-parsed piece-file document against the schema built
// from model. Throws DungeonError (with the offending schema keyword and a
// JSON-pointer to the offending value) if the document does not conform.
void ValidatePieceDocument(const rapidjson::Document& document, const PieceSchemaModel& model);

} // namespace psr
