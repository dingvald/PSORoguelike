#include "Items/DropTableRoller.h"

namespace psr {

namespace {

    const DropTableEntry* WeightedPick(const std::vector<DropTableEntry>& pool, SectionId section_id, std::mt19937& rng)
    {
        float total_weight = 0.0f;
        for (const DropTableEntry& entry : pool)
            total_weight += entry.SectionWeight(section_id);
        if (total_weight <= 0.0f)
            return nullptr;

        std::uniform_real_distribution<float> pick_roll(0.0f, total_weight);
        float roll = pick_roll(rng);
        for (const DropTableEntry& entry : pool)
        {
            const float weight = entry.SectionWeight(section_id);
            if (roll < weight)
                return &entry;
            roll -= weight;
        }
        return &pool.back(); // rounding-edge fallback
    }

} // namespace

DropTableResult Roll(const DropTable& table, SectionId section_id, std::mt19937& rng)
{
    DropTableResult result;
    result.item_prefab_ids = table.guaranteed_item_ids;

    std::uniform_real_distribution<float> gate_roll(0.0f, 100.0f);
    const bool roll_rare = gate_roll(rng) < table.rare_roll_chance_percent;
    const std::vector<DropTableEntry>& pool = roll_rare ? table.rare_entries : table.common_entries;

    if (const DropTableEntry* picked = WeightedPick(pool, section_id, rng))
        result.item_prefab_ids.push_back(picked->item_prefab_id);

    if (table.meseta_max > 0)
    {
        std::uniform_int_distribution<int> meseta_roll(table.meseta_min, table.meseta_max);
        result.meseta = meseta_roll(rng);
    }

    return result;
}

} // namespace psr
