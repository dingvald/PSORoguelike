#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace psr {

// Resolved Techniques/Photon Arts screen contents for HudLayer to render --
// display names only (same "fully resolved" contract CharacterScreenMessage
// documents), plus the raw ids each row needs to round-trip through a
// TechniquesScreenSlotAssignedMessage. Unlike CharacterScreenMessage's
// inventory (assigned by inventory index), there is no index concept here --
// the id itself is the natural key, same thing HotbarSlot::id already
// stores. Published by TechniquesScreenState::OnEnter (see
// Items/TechniquesScreenSnapshot.h's BuildTechniquesScreenMessage).
struct TechniquesScreenMessage
{
    struct TechniqueEntry
    {
        std::string display_name;
        std::uint32_t technique_id = 0;
        int tier = 1;

        // Absolute filesystem path to a 16x16 icon PNG, or empty if none
        // exists on disk for this technique's id -- see
        // BuildTechniquesScreenMessage's own doc comment.
        std::string icon_path;
    };

    struct PhotonArtEntry
    {
        std::string display_name;
        std::uint32_t photon_art_id = 0;
    };

    // Every Technique the player has learned (KnownTechniquesComponent).
    std::vector<TechniqueEntry> techniques;

    // Every Photon Art the currently-equipped weapon grants -- read-only
    // knowledge, not something this screen can teach.
    std::vector<PhotonArtEntry> photon_arts;
};

} // namespace psr
