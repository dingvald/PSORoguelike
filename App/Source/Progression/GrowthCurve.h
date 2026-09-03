#pragma once

#include "Components/StatsComponent.h"

#include <vector>

namespace psr {

// One authored level's absolute stat totals -- not a delta from the
// previous level, so applying a level-up is a plain overwrite with no
// incremental arithmetic to get wrong. xp_to_next is the XP required,
// starting from level-1, to reach this level.
struct GrowthCurveLevel
{
    int level = 0;
    int xp_to_next = 0;
    int max_hp = 0;
    int max_tp = 0;
    StatsComponent stats;
};

// The character's level-up table: one class-agnostic curve for now (see
// docs/GDD.md's per-class growth curve -- character classes don't exist yet,
// so there's nothing to key a per-class curve on). A single hand-authored
// JSON document (App/Assets/Data/growth_curve.json), not a per-item content
// library like Techniques/PhotonArts -- see GrowthCurveFile.h for the loader.
struct GrowthCurve
{
    std::vector<GrowthCurveLevel> levels; // sorted ascending by level, starting at 2

    // nullptr if `level` is past the authored curve (no further leveling).
    const GrowthCurveLevel* Find(int level) const;
};

} // namespace psr
