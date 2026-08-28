#pragma once

#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <string_view>
#include <utility>

namespace psr {

// The fixed elemental roster (docs/GDD.md's PSO-analogous Fire/Ice/Lightning/
// Light/Dark). Technique.h previously carried element_id as a free-form NameId
// ("the GDD declines to commit to a fixed element roster") -- M7.3 commits to
// exactly these five plus None, so a real enum replaces that NameId. Purely
// informational for now: nothing reads Element to compute a resistance/
// multiplier yet (out of scope this pass, see StatusEffect.h), it only flows
// through Before<Action>Event so a future resistance pass has real data to
// key off.
enum class Element
{
    None,
    Fire,
    Ice,
    Lightning,
    Light,
    Dark
};

template <> struct EnumNames<Element>
{
    static constexpr std::array<std::pair<std::string_view, Element>, 6> kValues{{
        {"none", Element::None},
        {"fire", Element::Fire},
        {"ice", Element::Ice},
        {"lightning", Element::Lightning},
        {"light", Element::Light},
        {"dark", Element::Dark},
    }};
};

} // namespace psr
