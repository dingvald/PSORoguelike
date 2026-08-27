#include "Engine/Items/DropResolution.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace psr {

namespace {

    // A rare entry favoring the roller's own Section ID is this much more
    // likely to be picked over an unfavored (None) entry -- a placeholder
    // multiplier, real tuning deferred to a future balancing pass per
    // docs/GDD.md, same treatment CombatMath.h already gives its own
    // undocumented-in-source constants.
    constexpr float kSectionIdMatchBonus = 3.0f;

    const DropEntry* PickWeightedEntry(const std::vector<DropEntry>& entries, SectionId roller_section,
                                       std::mt19937& rng)
    {
        std::vector<std::pair<const DropEntry*, float>> eligible;
        float total_weight = 0.0f;
        for (const DropEntry& entry : entries)
        {
            // Locked to a different Section ID than the roller's -- not
            // eligible at all.
            if (entry.favored_section_id != SectionId::None && entry.favored_section_id != roller_section)
                continue;

            float weight = static_cast<float>(entry.weight);
            if (entry.favored_section_id != SectionId::None && entry.favored_section_id == roller_section)
                weight *= kSectionIdMatchBonus;
            if (weight <= 0.0f)
                continue;

            eligible.emplace_back(&entry, weight);
            total_weight += weight;
        }

        if (eligible.empty() || total_weight <= 0.0f)
            return nullptr;

        std::uniform_real_distribution<float> pick_roll(0.0f, total_weight);
        float pick = pick_roll(rng);
        for (const auto& [entry, weight] : eligible)
        {
            pick -= weight;
            if (pick <= 0.0f)
                return entry;
        }
        return eligible.back().first; // floating-point rounding fallback
    }

} // namespace

DropResult ResolveDrop(const DropTable& table, SectionId roller_section, std::mt19937& rng)
{
    DropResult result;

    if (table.meseta_max > 0)
    {
        std::uniform_int_distribution<int> meseta_roll(std::min(table.meseta_min, table.meseta_max), table.meseta_max);
        result.meseta = meseta_roll(rng);
    }

    std::uniform_real_distribution<float> chance_roll(0.0f, 100.0f);
    const bool rolled_rare =
        table.boss_guaranteed_rare || chance_roll(rng) < static_cast<float>(table.rare_chance_percent);

    const DropEntry* picked = rolled_rare ? PickWeightedEntry(table.rare_entries, roller_section, rng) : nullptr;
    if (!picked)
        picked = PickWeightedEntry(table.common_entries, roller_section, rng);

    if (picked)
        result.item_prefab_id = picked->item_prefab_id;

    return result;
}

} // namespace psr
