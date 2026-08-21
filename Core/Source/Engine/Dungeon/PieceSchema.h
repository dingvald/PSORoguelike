#pragma once

#include "Engine/ECS/ComponentSchema.h"

#include <vector>

namespace psr {

// The whole authorable surface of one piece file: name/area_tag/category plus
// the cells array (each cell's own fields, recursively reflected from
// PieceCell/PieceCellPrefab). Consumed by PieceSchemaEmitter to emit/validate
// the pieces JSON Schema, exactly as EntitySchemaModel feeds
// EntitySchemaEmitter and BiomeSchemaModel feeds BiomeSchemaEmitter in
// UnnamedRoguelike.
struct PieceSchemaModel
{
    std::vector<FieldSchema> fields;
};

// Reflects the piece data model (PieceCell/PieceCellPrefab's Describe hooks)
// into a PieceSchemaModel. Pure and self-contained -- no world/registry
// needed -- so the emitter, the loader's validation step, and any future CLI
// schema dump all build the same model.
PieceSchemaModel BuildPieceSchemaModel();

} // namespace psr
