#pragma once

#include "Engine/Items/DropTable.h"
#include "Engine/Items/DropTableLibrary.h"

#include <rapidjson/document.h>

#include <filesystem>

namespace psr {

// Drop Table file schema_version. Independent of every other content
// family's own version.
inline constexpr int kDropTableLibraryVersion = 1;

// Parses one Drop-Table-shaped JSON body into a DropTable with id/id_string
// left default. Caller must validate the source document first
// (ValidateDropTableDocument applies the same per-body schema this reads).
DropTable ReadDropTableBody(const rapidjson::Value& drop_table_def);

// Inverse of ReadDropTableBody. Shared by SaveDropTable.
rapidjson::Value WriteDropTableBody(const DropTable& drop_table, rapidjson::Document::AllocatorType& allocator);

// Recursively loads every *.json under directory (one Drop Table per file)
// into a DropTableLibrary. A Drop Table's id is its path relative to
// directory, same convention as LoadPhotonArtLibrary/LoadAffixLibrary.
// Throws DropTableError on a schema-version mismatch or malformed entry,
// JsonFileError on a missing directory or an unparseable file.
DropTableLibrary LoadDropTableLibrary(const std::filesystem::path& directory);

// Writes drop_table to path as a single Drop Table file -- the inverse of
// one LoadDropTableLibrary entry. drop_table.id/id_string are not written.
// Throws DropTableError if the built document fails ValidateDropTableDocument.
void SaveDropTable(const std::filesystem::path& path, const DropTable& drop_table);

} // namespace psr
