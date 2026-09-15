#pragma once

#include "Progression/ClassDefinition.h"

#include <filesystem>

namespace psr {

inline constexpr int kClassDefinitionVersion = 1;

// Loads one class's App/Assets/Data/Classes/<class-id>.json document --
// ReadJsonFile(path, kClassDefinitionVersion) directly, not LoadJsonDirectory,
// same "one hand-authored document, not a per-item content library" shape
// growth_curve.json used before ClassId existed (see ClassDefinition.h).
// Throws JsonFileError on any malformed field, or if the file's own
// "class_id" string doesn't resolve to `expected` (e.g. a copy-pasted file
// with the wrong id left in place).
ClassDefinition LoadClassDefinition(const std::filesystem::path& path, ClassId expected);

} // namespace psr
