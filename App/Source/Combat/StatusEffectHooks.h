#pragma once

#include "Engine/ECS/Entity.h"

#include <cstdint>
#include <random>

namespace psr {

class StatusEffectLibrary;

// Rolls chance_percent and, on success, calls ApplyStatusEffect(target,
// library, status_effect_id) -- the shared "elemental damage has a chance to
// cause an effect" hook AttackAction/PhotonArtAction/TechniqueAction each
// call once, right after a hit lands and damage is applied. No-ops silently
// if status_effect_id == 0 or chance_percent <= 0 (the common case: most
// weapons/Techniques are non-elemental).
void MaybeApplyElementalStatus(Entity target, const StatusEffectLibrary& library, std::uint32_t status_effect_id,
                               int chance_percent, std::mt19937& rng);

} // namespace psr
