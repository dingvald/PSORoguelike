#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <string_view>
#include <utility>

namespace psr {

// Which of the four equip locations an armor prefab occupies. A structural
// game-mechanic concept (the character's body plan), not open-ended theme
// content like RaceComponent's races -- fixed by design, per CLAUDE.md's
// allowance for App-flavored vocabulary encoded directly (this still lives
// in Core since ArmorComponent itself has no App-side system coupling).
enum class ArmorSlot
{
    Head,
    Torso,
    Hands,
    Legs
};

template <> struct EnumNames<ArmorSlot>
{
    static constexpr std::array<std::pair<std::string_view, ArmorSlot>, 4> kValues{{
        {"head", ArmorSlot::Head},
        {"torso", ArmorSlot::Torso},
        {"hands", ArmorSlot::Hands},
        {"legs", ArmorSlot::Legs},
    }};
};

// An armor prefab's non-stat fields. An armor entity also carries a sibling
// StatsComponent (the bonus it grants when equipped) and RarityComponent.
//
// mod_slot_count (0-4, a flat cap for every armor piece, more generous than
// PSO's variable 0-4) is the template's *slot capacity* only -- which live
// mod instance currently occupies which slot is runtime equip state with no
// consumer yet (no mod-effect system, no inventory UI), so it isn't modeled
// here. Nothing in this schema enforces the 0-4 bound generically (no
// FieldKind has a min/max concept); the Prefab Editor enforces it with a
// dropdown instead of a free-entry int field.
struct ArmorComponent
{
    ArmorSlot slot = ArmorSlot::Torso;
    int mod_slot_count = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<ArmorComponent>("armor")
            .Data<&ArmorComponent::slot>("slot")
            .Data<&ArmorComponent::mod_slot_count>("mod_slot_count");
    }
};

} // namespace psr
