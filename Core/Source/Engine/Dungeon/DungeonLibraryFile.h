#pragma once

#include "Engine/Dungeon/Dungeon.h"
#include "Engine/Dungeon/DungeonLibrary.h"

#include <rapidjson/document.h>

#include <filesystem>

namespace psr {

// Dungeon file schema_version. Independent of the piece file's own version.
inline constexpr int kDungeonLibraryVersion = 1;

// Parses one dungeon-shaped JSON body into a Dungeon with id/id_string left
// default. Caller must validate the source document first
// (ValidateDungeonDocument applies the same per-body schema this reads).
Dungeon ReadDungeonBody(const rapidjson::Value& dungeon_def);

// Inverse of ReadDungeonBody. Shared by SaveDungeon.
rapidjson::Value WriteDungeonBody(const Dungeon& dungeon, rapidjson::Document::AllocatorType& allocator);

// Recursively loads every *.json under directory (one dungeon per file) into
// a DungeonLibrary. A dungeon's id is its path relative to directory, same
// convention as LoadPieceLibrary. Throws DungeonError on a schema-version
// mismatch or malformed entry, JsonFileError on a missing directory or an
// unparseable file.
DungeonLibrary LoadDungeonLibrary(const std::filesystem::path& directory);

// Writes dungeon to path as a single dungeon file -- the inverse of one
// LoadDungeonLibrary entry. dungeon.id/id_string are not written. Throws
// DungeonError if the built document fails ValidateDungeonDocument.
void SaveDungeon(const std::filesystem::path& path, const Dungeon& dungeon);

} // namespace psr
