#include "Engine/Combat/CombatMath.h"

#include <algorithm>
#include <cmath>

namespace psr {

namespace {
    constexpr float kMinHitChance = 0.05f;
    constexpr float kMaxHitChance = 0.95f;
} // namespace

float ComputeHitChance(int attacker_ata, int defender_evp)
{
    const float ata = static_cast<float>(std::max(0, attacker_ata));
    const float evp = static_cast<float>(std::max(0, defender_evp));
    if (ata + evp <= 0.0f)
        return kMaxHitChance;
    return std::clamp(ata / (ata + evp), kMinHitChance, kMaxHitChance);
}

int ComputeDamage(int attacker_atp, int defender_dfp, float variance_roll)
{
    const float raw = static_cast<float>(attacker_atp) - static_cast<float>(defender_dfp) / 2.0f;
    const int damage = static_cast<int>(std::lround(raw * variance_roll));
    return std::max(1, damage);
}

int ApplyRaceBonus(int damage, const std::vector<RaceBonusEntry>& race_bonuses, std::uint32_t defender_race_id)
{
    for (const RaceBonusEntry& bonus : race_bonuses)
    {
        if (bonus.race_id != defender_race_id)
            continue;
        return static_cast<int>(std::lround(static_cast<float>(damage) * (1.0f + bonus.bonus_percent / 100.0f)));
    }
    return damage;
}

} // namespace psr
