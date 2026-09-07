#pragma once

#include "Areas/Area.h"
#include "Areas/AreaLibrary.h"

#include <rapidjson/document.h>

#include <filesystem>

namespace psr {

// Area file schema_version. Independent of the affix/piece/dungeon files'
// own versions.
inline constexpr int kAreaLibraryVersion = 1;

// Parses one area-shaped JSON body into an Area with id/id_string left
// default. Caller must validate the source document first
// (ValidateAreaDocument applies the same per-body schema this reads).
Area ReadAreaBody(const rapidjson::Value& area_def);

// Inverse of ReadAreaBody. Shared by SaveArea.
rapidjson::Value WriteAreaBody(const Area& area, rapidjson::Document::AllocatorType& allocator);

// Recursively loads every *.json under directory (one area per file) into an
// AreaLibrary. An area's id is its path relative to directory, same
// convention as LoadAffixLibrary/LoadDungeonLibrary; its `tag` defaults to
// that same id when left unauthored (see ReadAreaBody). Throws AreaError on
// a schema-version mismatch or malformed entry, JsonFileError on a missing
// directory or an unparseable file.
AreaLibrary LoadAreaLibrary(const std::filesystem::path& directory);

// Writes area to path as a single area file -- the inverse of one
// LoadAreaLibrary entry. area.id/id_string are not written. Throws AreaError
// if the built document fails ValidateAreaDocument.
void SaveArea(const std::filesystem::path& path, const Area& area);

} // namespace psr
