#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/SectionId.h"

namespace psr {

// A character's Section ID (docs/GDD.md: "a character-creation choice...
// that skews which rare items/enemies can appear"). Authored as a template
// default on player.json until M10.3 (character creation) exists to let the
// player actually pick one at runtime -- same "template default, real
// selection logic deferred" idiom WeaponComponent::grind_level already
// uses. Consumed by DropResolution.cpp via LootDropSystem.
struct SectionIdComponent
{
    SectionId section_id = SectionId::None;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<SectionIdComponent>("section_id").Data<&SectionIdComponent::section_id>("section_id");
    }
};

} // namespace psr
