#pragma once

#include "Engine/Dungeon/DungeonPiece.h"
#include "Engine/Dungeon/PieceLibrary.h"

#include <rapidjson/document.h>

#include <filesystem>

namespace psr {

// Piece file schema_version. Independent of any other file's own version so
// each can bump without forcing the other (mirrors UnnamedRoguelike's
// per-file SaveVersion.h style).
inline constexpr int kPieceLibraryVersion = 1;

// Parses one piece-shaped JSON body -- name/area_tag/category/cells -- into a
// DungeonPiece with id/id_string left default. A piece file's own id comes
// from its path (see LoadPieceLibrary below). Caller must validate the
// source document first (ValidatePieceDocument applies the same per-body
// schema this reads).
DungeonPiece ReadPieceBody(const rapidjson::Value& piece_def);

// Inverse of ReadPieceBody: writes piece's body fields (everything but
// id/id_string/schema_version) as a JSON object value. Shared by SavePiece.
rapidjson::Value WritePieceBody(const DungeonPiece& piece, rapidjson::Document::AllocatorType& allocator);

// Recursively loads every *.json under directory (one piece per file) into a
// PieceLibrary. A piece's id is its path relative to directory, with '/'
// replaced by '.' and ".json" stripped -- e.g. "forest_corridor.json" ->
// "forest_corridor". Throws DungeonError on a schema-version mismatch or
// malformed entry, JsonFileError on a missing directory or an unparseable
// file.
PieceLibrary LoadPieceLibrary(const std::filesystem::path& directory);

// Writes piece to path as a single piece file -- the inverse of one
// LoadPieceLibrary entry. piece.id/id_string are not written (a piece's id
// comes from its path, not its content -- see LoadPieceLibrary). Throws
// DungeonError if the built document fails ValidatePieceDocument (so a save
// can never produce a file LoadPieceLibrary would reject).
void SavePiece(const std::filesystem::path& path, const DungeonPiece& piece);

} // namespace psr
