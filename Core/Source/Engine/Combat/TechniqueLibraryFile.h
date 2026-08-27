#pragma once

#include "Engine/Combat/Technique.h"
#include "Engine/Combat/TechniqueLibrary.h"

#include <rapidjson/document.h>

#include <filesystem>

namespace psr {

// Technique file schema_version. Independent of every other content family's
// own version.
inline constexpr int kTechniqueLibraryVersion = 1;

// Parses one Technique-shaped JSON body into a Technique with id/id_string
// left default. Caller must validate the source document first
// (ValidateTechniqueDocument applies the same per-body schema this reads).
Technique ReadTechniqueBody(const rapidjson::Value& technique_def);

// Inverse of ReadTechniqueBody. Shared by SaveTechnique.
rapidjson::Value WriteTechniqueBody(const Technique& technique, rapidjson::Document::AllocatorType& allocator);

// Recursively loads every *.json under directory (one Technique per file)
// into a TechniqueLibrary. A Technique's id is its path relative to
// directory, same convention as LoadAffixLibrary/LoadPhotonArtLibrary. Throws
// TechniqueError on a schema-version mismatch or malformed entry,
// JsonFileError on a missing directory or an unparseable file.
TechniqueLibrary LoadTechniqueLibrary(const std::filesystem::path& directory);

// Writes technique to path as a single Technique file -- the inverse of one
// LoadTechniqueLibrary entry. technique.id/id_string are not written. Throws
// TechniqueError if the built document fails ValidateTechniqueDocument.
void SaveTechnique(const std::filesystem::path& path, const Technique& technique);

} // namespace psr
