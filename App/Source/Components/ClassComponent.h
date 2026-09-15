#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Progression/CharacterClass.h"

namespace psr {

// The player's fixed class for this run (Hunter/Ranger/Force, see
// CharacterClass.h) -- chosen at character creation, never re-chosen. A real
// enum rather than a NameId, same fixed-roster reasoning as SectionIdComponent.
// Schema-registered for consistency with that precedent even though nothing
// authors it in a prefab -- it is only ever emplaced on the player in code.
struct ClassComponent
{
    ClassId class_id = ClassId::Hunter;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<ClassComponent>("class").Data<&ClassComponent::class_id>("class_id");
    }
};

} // namespace psr
