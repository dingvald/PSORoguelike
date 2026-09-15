#pragma once

#include "Hub/HubDefinition.h"

#include <filesystem>

namespace psr {

inline constexpr int kHubDefinitionVersion = 1;

// Loads the single hand-authored hub.json document (see HubDefinition.h) --
// ReadJsonFile(path, kHubDefinitionVersion) directly, not LoadJsonDirectory,
// since this is one document with its own schema, not a per-item content
// library. Throws JsonFileError (the same type ReadJsonFile itself throws
// for file/parse/schema_version problems) on a malformed document -- there's
// no editor round-tripping this file, so a clear exception message is the
// validation story, same precedent ClassDefinitionFile.h already set.
HubDefinition LoadHubDefinition(const std::filesystem::path& path);

} // namespace psr
