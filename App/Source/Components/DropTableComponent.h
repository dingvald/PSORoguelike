#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <cstdint>
#include <vector>

namespace psr {

// One weighted item-prefab entry in a drop table's pool -- see
// DropTableComponent's own doc comment for how it's combined with the
// no-drop/meseta weights. Describable (not a component itself), same
// "nested value type inside an authorable vector field" shape as
// WeaponComponent::RaceBonusEntry.
struct LootEntry
{
    std::uint32_t item_prefab_id = 0;
    float weight = 1.0f;

    template <typename V> static void Describe(V& v)
    {
        v.template Field<&LootEntry::item_prefab_id>("item_prefab_id");
        v.template Field<&LootEntry::weight>("weight");
    }
};

// An enemy/boss prefab's own loot table, authored inline (no separate
// library/id indirection -- unlike RaceComponent::race_id, this data lives
// entirely on the prefab). DropTableRoller::Roll treats entries,
// no_drop_weight, and meseta_weight as one flat weighted pool and picks
// exactly one outcome per kill: no_drop_weight favors dropping nothing,
// meseta_weight favors a Meseta pickup (amount uniform over
// [meseta_min, meseta_max]), and each entries[i] favors spawning that
// item_prefab_id. Enemies with no DropTableComponent (or an all-zero-weight
// one) simply drop nothing.
struct DropTableComponent
{
    std::vector<LootEntry> entries;
    float no_drop_weight = 0.0f;
    float meseta_weight = 0.0f;
    int meseta_min = 0;
    int meseta_max = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<DropTableComponent>("drop_table")
            .Data<&DropTableComponent::entries>("entries")
            .Data<&DropTableComponent::no_drop_weight>("no_drop_weight")
            .Data<&DropTableComponent::meseta_weight>("meseta_weight")
            .Data<&DropTableComponent::meseta_min>("meseta_min")
            .Data<&DropTableComponent::meseta_max>("meseta_max");
    }
};

} // namespace psr
