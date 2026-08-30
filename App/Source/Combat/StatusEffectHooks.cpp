#include "Combat/StatusEffectHooks.h"

#include "Combat/StatusEffectApplication.h"

namespace psr {

void MaybeApplyElementalStatus(Entity target, const StatusEffectLibrary& library, std::uint32_t status_effect_id,
                               int chance_percent, std::mt19937& rng)
{
    if (status_effect_id == 0 || chance_percent <= 0)
        return;

    std::uniform_real_distribution<float> unit_roll(0.0f, 1.0f);
    if (unit_roll(rng) * 100.0f > static_cast<float>(chance_percent))
        return; // missed the roll

    ApplyStatusEffect(target, library, status_effect_id);
}

} // namespace psr
