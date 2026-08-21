#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <rapidjson/document.h>

namespace psr {

// One parsed file discovered by LoadJsonDirectory. id is the file's path
// relative to the scanned directory, with '/' replaced by '.' and
// filename_suffix stripped -- e.g. "terrain/floor.json" under an "Entities"
// directory becomes "terrain.floor".
struct JsonDirectoryEntry
{
    std::string id;
    std::filesystem::path path;
    rapidjson::Document document;
};

// Recursively scans directory for files whose filename ends with
// filename_suffix (matched as a literal suffix, so multi-part extensions
// like ".map.json" work), parsing each via ReadJsonFile(path,
// expected_schema_version) -- so a malformed fragment or a schema_version
// mismatch throws JsonFileError, same as a single-file load would. Entries
// are returned sorted by id, since directory-iteration order is
// filesystem-dependent and callers (duplicate-id checks, tests) need a
// deterministic order. Throws JsonFileError if directory doesn't exist.
std::vector<JsonDirectoryEntry> LoadJsonDirectory(const std::filesystem::path& directory, int expected_schema_version,
                                                  const std::string& filename_suffix = ".json");

} // namespace psr
