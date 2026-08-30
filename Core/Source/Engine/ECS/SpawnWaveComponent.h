#pragma once

#include <cstdint>

namespace psr {

// Runtime-only tag identifying which spawn-wave group (a piece placement)
// and wave number an entity belongs to. Stamped only by DungeonInstantiator/
// SpawnWaveSystem when a wave's entities are created -- never authored,
// never cloned via a prefab, deliberately not passed through
// ComponentSchemaRegistrar (mirrors EquipmentComponent/StatusEffectComponent).
struct SpawnWaveComponent
{
    std::uint32_t group_id = 0;
    int wave = 0;
};

} // namespace psr
