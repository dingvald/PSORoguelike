#pragma once

#include "Items/DropTable.h"
#include "Items/DropTableLibrary.h"

#include <rapidjson/document.h>

#include <filesystem>

namespace psr {

// Drop-table file schema_version. Independent of the other content files'
// own versions.
inline constexpr int kDropTableLibraryVersion = 1;

// Parses one drop-table-shaped JSON body into a DropTable with id/id_string
// left default. Caller must validate the source document first
// (ValidateDropTableDocument applies the same per-body schema this reads).
DropTable ReadDropTableBody(const rapidjson::Value& drop_table_def);

// Inverse of ReadDropTableBody. Shared by SaveDropTable.
rapidjson::Value WriteDropTableBody(const DropTable& drop_table, rapidjson::Document::AllocatorType& allocator);

// Recursively loads every *.json under directory (one drop table per file)
// into a DropTableLibrary. A table's id is its path relative to directory,
// same convention as LoadAffixLibrary/LoadPieceLibrary. Throws DropTableError
// on a schema-version mismatch or malformed entry, JsonFileError on a missing
// directory or an unparseable file.
DropTableLibrary LoadDropTableLibrary(const std::filesystem::path& directory);

// Writes drop_table to path as a single drop-table file -- the inverse of one
// LoadDropTableLibrary entry. drop_table.id/id_string are not written. Throws
// DropTableError if the built document fails ValidateDropTableDocument.
void SaveDropTable(const std::filesystem::path& path, const DropTable& drop_table);

} // namespace psr
