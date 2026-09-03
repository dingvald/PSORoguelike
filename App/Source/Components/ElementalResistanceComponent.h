#pragma once

#include "Combat/Element.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// Per-element damage resistance, expressed as a percent (0-100, clamped
// where read) reduction applied to Technique damage of the matching element
// -- PSO techniques bypass DFP entirely and are mitigated by this instead
// (see CombatMath.h's ComputeTechniqueDamage). Optional: absent on an entity
// (or Element::None) reads as zero resistance. No default values beyond zero
// are implied here -- real per-entity numbers are authored content.
struct ElementalResistanceComponent
{
    int fire = 0;
    int ice = 0;
    int lightning = 0;
    int light = 0;
    int dark = 0;

    int ResistanceFor(Element element) const
    {
        switch (element)
        {
        case Element::Fire:
            return fire;
        case Element::Ice:
            return ice;
        case Element::Lightning:
            return lightning;
        case Element::Light:
            return light;
        case Element::Dark:
            return dark;
        case Element::None:
            return 0;
        }
        return 0;
    }

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<ElementalResistanceComponent>("elemental_resistance")
            .Data<&ElementalResistanceComponent::fire>("fire")
            .Data<&ElementalResistanceComponent::ice>("ice")
            .Data<&ElementalResistanceComponent::lightning>("lightning")
            .Data<&ElementalResistanceComponent::light>("light")
            .Data<&ElementalResistanceComponent::dark>("dark");
    }
};

} // namespace psr
