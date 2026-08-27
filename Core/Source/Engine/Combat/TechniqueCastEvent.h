#pragma once

#include <cstdint>

namespace psr {

// Dispatched by TechniqueAction to the caster's own EventHandlerComponent
// (Entity::Dispatch), once per successful cast (TP actually spent) --
// separate from any BeforeDamageEvent/AfterDamageEvent the same cast may
// also dispatch for its damage effect.
struct BeforeTechniqueCastEvent
{
    std::uint32_t technique_id = 0;
};

struct AfterTechniqueCastEvent
{
    std::uint32_t technique_id = 0;
};

} // namespace psr
