#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace psr {

// The GDD's four canonical enemy races (docs/GDD.md's "four-race damage
// system": Native/A.Beast/Machine/Dark) -- RaceComponent::race_id stays a
// free-form NameId string rather than a fixed enum (see RaceComponent.h),
// but EquipmentDropRoller (picking a species bonus to roll) and
// CharacterScreenSnapshot (resolving one back to a display label) both need
// the same canonical {id string, display name} pairs, so they live here
// once instead of two drifting copies.
inline constexpr std::array<std::pair<std::string_view, std::string_view>, 4> kCanonicalRaces{{
    {"native", "Native"},
    {"a_beast", "A.Beast"},
    {"machine", "Machine"},
    {"dark", "Dark"},
}};

} // namespace psr
