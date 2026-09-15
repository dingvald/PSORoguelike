#pragma once

#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

namespace psr {

inline constexpr std::size_t kClassIdCount = 3;

// The fixed Hunter/Ranger/Force triad (docs/GDD.md's "Class triad") -- same
// shape as SectionId.h: a small, permanently-named set, not open-ended
// content, so a real enum replaces what would otherwise be a NameId.
enum class ClassId
{
    Hunter,
    Ranger,
    Force
};

template <> struct EnumNames<ClassId>
{
    static constexpr std::array<std::pair<std::string_view, ClassId>, 3> kValues{{
        {"hunter", ClassId::Hunter},
        {"ranger", ClassId::Ranger},
        {"force", ClassId::Force},
    }};
};

} // namespace psr
