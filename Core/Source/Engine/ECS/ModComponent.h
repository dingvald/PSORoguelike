#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// Marks a prefab as a mod (PSO-style Unit) -- an item that plugs into one of
// an armor piece's mod_slot_count slots. A mod entity carries this marker
// plus a sibling StatsComponent (its stat contribution) and RarityComponent.
// Which slot a mod instance currently occupies is runtime equip state with
// no consumer yet, so it isn't modeled here (see ArmorComponent's note).
struct ModComponent
{
    static void Register(ComponentSchemaRegistrar& reg) { reg.Component<ModComponent>("mod"); }
};

} // namespace psr
