#pragma once

#include "Engine/ECS/StatsComponent.h"
#include "Engine/ECS/WeaponComponent.h" // RaceBonusEntry

#include <cstdint>
#include <vector>

namespace psr {

// Dispatched by PhotonArtAction to the caster's own EventHandlerComponent
// (Entity::Dispatch) at the very start of Perform() -- see
// TechniqueCastEvent.h for the full rationale (both spend the same
// TPComponent pool and follow the identical gather-then-gate pattern).
struct BeforePhotonArtCastEvent
{
    std::uint32_t photon_art_id = 0;
    bool has_weapon = false;
    bool weapon_grants_id = false;
    int current_tp = 0;
    bool has_tp_component = false;
    std::vector<RaceBonusEntry> race_bonuses;
    StatsComponent attacker_stats;
};

struct AfterPhotonArtCastEvent
{
    std::uint32_t photon_art_id = 0;
};

} // namespace psr
