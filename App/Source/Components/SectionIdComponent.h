#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Items/SectionId.h"

namespace psr {

// The player's fixed Section ID for this run, biasing DropTableEntry weights
// via DropTableRoller. A real enum (see SectionId.h) rather than a NameId --
// PSO's 10 IDs are a permanently fixed roster, same shape as Element, not
// open-ended content like RaceComponent's races. Defaults to Viridia since
// M10.3 character creation doesn't exist yet to let the player choose one.
struct SectionIdComponent
{
    SectionId section_id = SectionId::Viridia;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<SectionIdComponent>("section_id").Data<&SectionIdComponent::section_id>("section_id");
    }
};

} // namespace psr
