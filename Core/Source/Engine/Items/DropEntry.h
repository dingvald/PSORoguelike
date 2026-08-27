#pragma once

#include "Engine/ECS/SectionId.h"

#include <cstdint>

namespace psr {

// One weighted entry in a DropTable's common/rare list. favored_section_id
// (docs/GDD.md's Section-ID weighting) is None by default (no bias, any
// Section ID can roll it); set to a real id to lock this entry to that
// Section ID's rollers (excluded for everyone else) while boosting its odds
// for them (see DropResolution.cpp's kSectionIdMatchBonus) -- PSO itself
// gates some rare/board items by Section ID this way, not just biasing every
// entry's weight uniformly. Shared by DropTable::common_entries and
// ::rare_entries (one struct, not two), mirroring PhotonArtTier's reuse
// across tiers.
struct DropEntry
{
    std::uint32_t item_prefab_id = 0;
    int weight = 1;
    SectionId favored_section_id = SectionId::None;

    template <typename V> static void Describe(V& v)
    {
        v.template Field<&DropEntry::item_prefab_id>("item_prefab_id");
        v.template Field<&DropEntry::weight>("weight");
        v.template Field<&DropEntry::favored_section_id>("favored_section_id");
    }
};

} // namespace psr
