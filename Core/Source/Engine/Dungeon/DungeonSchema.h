#pragma once

#include "Engine/ECS/ComponentSchema.h"

#include <vector>

namespace psr {

// The whole authorable surface of one dungeon file: name/area_tag plus the
// pieces/room-count/loopback-count/lock-count/door-prefab fields. Mirrors
// PieceSchemaModel's role for DungeonPiece.
struct DungeonSchemaModel
{
    std::vector<FieldSchema> fields;
};

// Reflects the dungeon data model (DungeonPieceRef's Describe hook, plus
// Dungeon's own plain scalar fields) into a DungeonSchemaModel. Pure and
// self-contained, same shape as BuildPieceSchemaModel.
DungeonSchemaModel BuildDungeonSchemaModel();

} // namespace psr
