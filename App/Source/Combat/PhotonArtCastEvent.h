#pragma once

#include "Combat/Element.h"
#include "Components/StatsComponent.h"
#include "Components/WeaponComponent.h" // RaceBonusEntry

#include <cstdint>
#include <vector>

namespace psr {

// Dispatched by PhotonArtAction to the caster's own EventHandlerComponent
// (Entity::Dispatch) at the very start of Perform() -- see
// TechniqueCastEvent.h for the full rationale (both spend the same
// TPComponent pool and follow the identical gather-then-gate pattern).
// element/status_effect_id/status_chance_percent are the wielded weapon's own
// elemental flavor (a Photon Art is "channeled through" its granting weapon,
// per Technique.h's own doc comment on how Photon Arts/Techniques are
// granted) -- separate from PhotonArt::status_effect_id, which stays
// reserved for its own EffectFamily::Status branch. StatusEffectComponent's
// own handler sets cancelled = true when the caster is Shocked.
struct BeforePhotonArtCastEvent
{
    std::uint32_t photon_art_id = 0;
    bool has_weapon = false;
    bool weapon_grants_id = false;
    int current_tp = 0;
    bool has_tp_component = false;
    std::vector<RaceBonusEntry> race_bonuses;
    StatsComponent attacker_stats;
    Element element = Element::None;
    std::uint32_t status_effect_id = 0;
    int status_chance_percent = 0;
    std::uint32_t hit_effect_prefab_id = 0; // weapon's OnHitEffectComponent, 0 = none
    float hit_effect_duration = 0.3f;
    bool cancelled = false;
};

struct AfterPhotonArtCastEvent
{
    std::uint32_t photon_art_id = 0;
};

} // namespace psr
