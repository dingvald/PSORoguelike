#include "Items/DropTableRoller.h"

namespace psr {

DropTableResult Roll(const DropTableComponent& table, std::mt19937& rng)
{
    float total_weight = table.no_drop_weight + table.meseta_weight;
    for (const LootEntry& entry : table.entries)
        total_weight += entry.weight;

    if (total_weight <= 0.0f)
        return DropTableResult{};

    std::uniform_real_distribution<float> pick_roll(0.0f, total_weight);
    float roll = pick_roll(rng);

    if (roll < table.no_drop_weight)
        return DropTableResult{};
    roll -= table.no_drop_weight;

    if (roll < table.meseta_weight)
    {
        std::uniform_int_distribution<int> meseta_roll(table.meseta_min, table.meseta_max);
        return DropTableResult{DropTableResult::Kind::Meseta, 0, meseta_roll(rng)};
    }
    roll -= table.meseta_weight;

    for (const LootEntry& entry : table.entries)
    {
        if (roll < entry.weight)
            return DropTableResult{DropTableResult::Kind::Item, entry.item_prefab_id, 0};
        roll -= entry.weight;
    }

    // Rounding-edge fallback: float error can leave `roll` a hair past the
    // last comparison above. Re-resolve against whichever pool member has
    // nonzero weight rather than indexing a possibly-empty entries vector.
    if (!table.entries.empty())
        return DropTableResult{DropTableResult::Kind::Item, table.entries.back().item_prefab_id, 0};
    if (table.meseta_weight > 0.0f)
    {
        std::uniform_int_distribution<int> meseta_roll(table.meseta_min, table.meseta_max);
        return DropTableResult{DropTableResult::Kind::Meseta, 0, meseta_roll(rng)};
    }
    return DropTableResult{};
}

} // namespace psr
