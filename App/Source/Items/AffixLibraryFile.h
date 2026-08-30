#pragma once

#include "Items/Affix.h"
#include "Items/AffixLibrary.h"

#include <rapidjson/document.h>

#include <filesystem>

namespace psr {

// Affix file schema_version. Independent of the piece/dungeon files' own
// versions.
inline constexpr int kAffixLibraryVersion = 1;

// Parses one affix-shaped JSON body into an Affix with id/id_string left
// default. Caller must validate the source document first
// (ValidateAffixDocument applies the same per-body schema this reads).
Affix ReadAffixBody(const rapidjson::Value& affix_def);

// Inverse of ReadAffixBody. Shared by SaveAffix.
rapidjson::Value WriteAffixBody(const Affix& affix, rapidjson::Document::AllocatorType& allocator);

// Recursively loads every *.json under directory (one affix per file) into
// an AffixLibrary. An affix's id is its path relative to directory, same
// convention as LoadPieceLibrary/LoadDungeonLibrary. Throws AffixError on a
// schema-version mismatch or malformed entry, JsonFileError on a missing
// directory or an unparseable file.
AffixLibrary LoadAffixLibrary(const std::filesystem::path& directory);

// Writes affix to path as a single affix file -- the inverse of one
// LoadAffixLibrary entry. affix.id/id_string are not written. Throws
// AffixError if the built document fails ValidateAffixDocument.
void SaveAffix(const std::filesystem::path& path, const Affix& affix);

} // namespace psr
