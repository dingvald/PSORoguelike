#include "Combat/StatusEffectHooks.h"

#include "Combat/CombatMath.h"
#include "Combat/StatusEffectApplication.h"

namespace psr {

int RollElementalDamageBonus(Entity target, const StatusEffectLibrary& library, Element element,
                             std::uint32_t status_effect_id, int chance_percent, int resistance_percent,
                             int attacker_atp, bool is_special_attack, std::mt19937& rng)
{
    if (element == Element::None)
        return 0;

    const int base_chance = is_special_attack ? 100 : chance_percent;
    const int effective_chance = ApplyResistanceToStatusChance(base_chance, resistance_percent);
    if (effective_chance <= 0)
        return 0;

    std::uniform_real_distribution<float> unit_roll(0.0f, 1.0f);
    if (unit_roll(rng) * 100.0f > static_cast<float>(effective_chance))
        return 0; // missed the roll

    if (status_effect_id != 0)
        ApplyStatusEffect(target, library, status_effect_id);

    return ComputeElementalDamage(attacker_atp, resistance_percent, is_special_attack);
}

} // namespace psr
