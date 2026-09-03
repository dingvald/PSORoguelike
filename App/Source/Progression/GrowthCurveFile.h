#pragma once

#include "Progression/GrowthCurve.h"

#include <filesystem>

namespace psr {

inline constexpr int kGrowthCurveVersion = 1;

// Loads the single hand-authored growth_curve.json document (see
// GrowthCurve.h) -- ReadJsonFile(path, kGrowthCurveVersion) directly, not
// LoadJsonDirectory, since this is one document with its own schema, not a
// per-item content library. Throws JsonFileError (the same type ReadJsonFile
// itself throws for file/parse/schema_version problems) on any malformed
// "levels" entry -- there's no editor round-tripping this file, so a clear
// exception message is the validation story.
GrowthCurve LoadGrowthCurve(const std::filesystem::path& path);

} // namespace psr
