#pragma once

#include <cstdint>

namespace psr {

// Dispatched by PhotonArtAction to the caster's own EventHandlerComponent
// (Entity::Dispatch), once per successful use (PP actually spent) --
// separate from any BeforeDamageEvent/AfterDamageEvent the same use may
// also dispatch for its damage effect. See TechniqueCastEvent.h for the
// Technique/TP equivalent.
struct BeforePhotonArtCastEvent
{
    std::uint32_t photon_art_id = 0;
};

struct AfterPhotonArtCastEvent
{
    std::uint32_t photon_art_id = 0;
};

} // namespace psr
