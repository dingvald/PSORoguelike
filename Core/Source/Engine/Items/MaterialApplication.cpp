#include "Engine/Items/MaterialApplication.h"

namespace psr {

void ApplyMaterial(StatsComponent& stats, HealthComponent& health, MaterialStat stat, int amount)
{
    switch (stat)
    {
    case MaterialStat::Atp:
        stats.atp += amount;
        break;
    case MaterialStat::Ata:
        stats.ata += amount;
        break;
    case MaterialStat::Mst:
        stats.mst += amount;
        break;
    case MaterialStat::Dfp:
        stats.dfp += amount;
        break;
    case MaterialStat::Evp:
        stats.evp += amount;
        break;
    case MaterialStat::Lck:
        stats.lck += amount;
        break;
    case MaterialStat::MaxHp:
        health.max_hp += amount;
        health.current_hp += amount;
        break;
    }
}

} // namespace psr
