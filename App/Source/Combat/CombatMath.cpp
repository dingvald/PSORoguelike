#include "Combat/CombatMath.h"

#include <algorithm>
#include <cmath>

namespace psr {

namespace {
    constexpr float kEvpAccuracyWeight = 0.2f;
    constexpr float kComboStepGrowth = 1.3f;
    constexpr float kDamageDivisor = 5.0f;
    constexpr float kDamageConstant = 0.9f;
    constexpr float kCritLckDivisor = 500.0f;
    constexpr float kCritMultiplier = 1.5f;
} // namespace

float ComputeHitChance(int attacker_ata, int defender_evp)
{
    const float ata = static_cast<float>(attacker_ata);
    const float evp = static_cast<float>(defender_evp);
    const float accuracy = ata - evp * kEvpAccuracyWeight;
    return std::clamp(accuracy, 0.0f, 100.0f) / 100.0f;
}

float ComboAtaMultiplier(int hit_index) { return std::pow(kComboStepGrowth, static_cast<float>(hit_index)); }

int ComputeDamage(int attacker_atp, int defender_dfp, float variance_roll)
{
    const float raw = static_cast<float>(attacker_atp) - static_cast<float>(defender_dfp);
    const int damage = static_cast<int>(std::floor(raw / kDamageDivisor * kDamageConstant * variance_roll));
    return std::max(1, damage);
}

int ComputeTechniqueDamage(int attacker_mst, int resistance_percent)
{
    const float resistance = std::clamp(static_cast<float>(resistance_percent), 0.0f, 100.0f);
    const float raw = static_cast<float>(attacker_mst) / kDamageDivisor * (1.0f - resistance / 100.0f);
    return std::max(1, static_cast<int>(std::floor(raw)));
}

float ComputeCritChance(int attacker_lck)
{
    return std::clamp(static_cast<float>(attacker_lck) / kCritLckDivisor, 0.0f, 1.0f);
}

int ApplyCritical(int damage, bool is_critical)
{
    if (!is_critical)
        return damage;
    return static_cast<int>(std::lround(static_cast<float>(damage) * kCritMultiplier));
}

int ApplyRaceBonus(int value, const std::vector<RaceBonusEntry>& race_bonuses, std::uint32_t defender_race_id)
{
    for (const RaceBonusEntry& bonus : race_bonuses)
    {
        if (bonus.race_id != defender_race_id)
            continue;
        return static_cast<int>(std::lround(static_cast<float>(value) * (1.0f + bonus.bonus_percent / 100.0f)));
    }
    return value;
}

} // namespace psr
