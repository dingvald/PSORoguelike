#pragma once

#include "Components/StatsComponent.h"

#include <rapidjson/document.h>

#include <random>

namespace psr {

// One evaluated level's absolute stat totals -- not a delta from the
// previous level, so applying a level-up is a plain overwrite with no
// incremental arithmetic to get wrong. xp_to_next is the XP required,
// starting from level-1, to reach this level. Computed by
// GrowthCurve::Evaluate, not hand-authored.
struct GrowthCurveLevel
{
    int level = 0;
    int xp_to_next = 0;
    int max_hp = 0;
    int max_tp = 0;
    StatsComponent stats;
};

// A single stat's growth formula: value(level) = base + rate * (level - 2)
// ^ exponent, so `base` is the value at level 2 (the first level-up) and
// `exponent == 1` (the default) is a plain linear per-level increase.
struct StatCurve
{
    float base = 0.0f;
    float rate = 0.0f;
    float exponent = 1.0f;
};

// A class's level-up curve: one independently-authored StatCurve per stat
// plus one for xp_to_next, evaluated on demand rather than looked up from a
// hand-authored per-level table -- see ParseGrowthCurve/ClassDefinitionFile.cpp
// for how these are loaded from Classes/<id>.json's "curves" object.
struct GrowthCurve
{
    StatCurve xp_to_next;
    StatCurve max_hp;
    StatCurve max_tp;
    StatCurve atp;
    StatCurve ata;
    StatCurve mst;
    StatCurve dfp;
    StatCurve evp;
    StatCurve lck;

    // level must be >= 2 -- level 1 stats come from the class's base_hp/base_tp
    // and the player prefab's own StatsComponent, not the growth curve.
    GrowthCurveLevel Evaluate(int level) const;

    // The random amount of each stat/HP/TP actually granted when the player
    // reaches `level`, for ExperienceSystem to add onto the player's current
    // totals -- not an absolute overwrite like Evaluate. Centered on this
    // curve's deterministic per-level-up gain (Evaluate(level) minus
    // Evaluate(level - 1), or each curve's own `rate` for the first level-up
    // at level 2, since level 1 isn't itself on the curve -- the two agree
    // exactly for a linear curve, which is every stat curve authored today),
    // each stat/HP/TP independently jittered by up to +/-25% (uniform, so
    // the long-run average matches the authored numbers). xp_to_next is left
    // untouched -- it's the level's XP cost, not a stat granted to the
    // player, so it stays deterministic.
    GrowthCurveLevel EvaluateRandomGain(int level, std::mt19937& rng) const;

private:
    GrowthCurveLevel EvaluateGain(int level) const;
};

// Parses a "curves" JSON object (see ClassDefinitionFile.cpp, this type's one
// call site) into a GrowthCurve. Throws JsonFileError on any malformed entry.
GrowthCurve ParseGrowthCurve(const rapidjson::Value& curves_object);

} // namespace psr
