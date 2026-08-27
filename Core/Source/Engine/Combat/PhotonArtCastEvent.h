#pragma once

#include <cstdint>

namespace psr {

// Dispatched by PhotonArtAction to the caster's own EventHandlerComponent
// (Entity::Dispatch), once per successful use (TP actually spent) --
// separate from any BeforeDamageEvent/AfterDamageEvent the same use may
// also dispatch for its damage effect. See TechniqueCastEvent.h for the
// Technique equivalent (both spend the same TPComponent pool).
struct BeforePhotonArtCastEvent
{
    std::uint32_t photon_art_id = 0;
};

struct AfterPhotonArtCastEvent
{
    std::uint32_t photon_art_id = 0;
};

} // namespace psr
