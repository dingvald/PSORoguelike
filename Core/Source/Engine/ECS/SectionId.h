#pragma once

#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <string_view>
#include <utility>

namespace psr {

// PSO's ten Section IDs (docs/GDD.md's "Itemization & Section IDs" --
// Viridia/Greenill/Skyly/Bluefull/Purplenum/Pinkal/Redria/Oran/Yellowboze/
// Whitill), plus a None sentinel (value 0, following this codebase's
// "0 = none" convention for every other id field) for "not chosen yet" --
// character creation (M10.3) doesn't exist yet to make a real choice, and
// DropResolution.cpp treats None as unbiased rather than defaulting to a
// real id.
enum class SectionId
{
    None,
    Viridia,
    Greenill,
    Skyly,
    Bluefull,
    Purplenum,
    Pinkal,
    Redria,
    Oran,
    Yellowboze,
    Whitill
};

template <> struct EnumNames<SectionId>
{
    static constexpr std::array<std::pair<std::string_view, SectionId>, 11> kValues{{
        {"none", SectionId::None},
        {"viridia", SectionId::Viridia},
        {"greenill", SectionId::Greenill},
        {"skyly", SectionId::Skyly},
        {"bluefull", SectionId::Bluefull},
        {"purplenum", SectionId::Purplenum},
        {"pinkal", SectionId::Pinkal},
        {"redria", SectionId::Redria},
        {"oran", SectionId::Oran},
        {"yellowboze", SectionId::Yellowboze},
        {"whitill", SectionId::Whitill},
    }};
};

} // namespace psr
