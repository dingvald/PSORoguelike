#pragma once

#include "Components/StatsComponent.h"

#include <filesystem>

namespace psr {

// Tunable EXP curve / per-level stat growth, authored as a single small JSON
// file (App/Assets/Data/leveling.json) rather than fixed engine constants --
// these are game-balance numbers the content owner should be able to retune
// without a rebuild. exp_growth_exponent > 1 grows the EXP curve faster than
// linear; stat_growth_per_level reuses StatsComponent itself as the 6-int
// growth bag (same fields, same meaning) rather than a bespoke struct.
struct LevelingConfig
{
    int exp_base = 100;
    float exp_growth_exponent = 1.5f;
    StatsComponent stat_growth_per_level;
};

// Loads leveling.json. Throws JsonFileError (see Engine/Persistence/JsonFile.h)
// on a missing file, malformed JSON, or schema_version mismatch -- same
// "content-load failures are build-input bugs" contract every other library
// loader (LoadDropTableLibrary, LoadStatusEffectLibrary, ...) already has.
LevelingConfig LoadLevelingConfig(const std::filesystem::path& path);

} // namespace psr
