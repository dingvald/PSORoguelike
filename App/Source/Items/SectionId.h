#pragma once

#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

namespace psr {

inline constexpr std::size_t kSectionIdCount = 10;

// The fixed 10-Section-ID roster PSO ties drop-table weighting to. Same shape
// as Element.h's fixed roster: a small, permanently-named set, not open-ended
// content like RaceComponent's races, so a real enum replaces what would
// otherwise be a NameId.
enum class SectionId
{
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
    static constexpr std::array<std::pair<std::string_view, SectionId>, 10> kValues{{
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
