#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace psr {

// Resolved Action Palette screen contents for HudLayer to render -- display
// names only (same "fully resolved" contract CharacterScreenMessage
// documents), plus the raw ids each row needs to round-trip through a
// ActionPaletteSlotAssignedMessage. Unlike CharacterScreenMessage's
// inventory (assigned by inventory index), there is no index concept here --
// the id itself is the natural key, same thing HotbarSlot::id already
// stores. Published by ActionPaletteState::OnEnter (see
// Items/ActionPaletteSnapshot.h's BuildActionPaletteMessage).
struct ActionPaletteMessage
{
    struct TechniqueEntry
    {
        std::string display_name;
        std::uint32_t technique_id = 0;
        int tier = 1;
        int tp_cost = 0;

        // Absolute filesystem path to a 16x16 icon PNG, or empty if none
        // exists on disk for this technique's id -- see
        // BuildActionPaletteMessage's own doc comment.
        std::string icon_path;
    };

    struct PhotonArtEntry
    {
        std::string display_name;
        std::uint32_t photon_art_id = 0;
        int tp_cost = 0;
    };

    // The equipped weapon's own basic attack -- unlike Technique/PhotonArt
    // entries there is no id to round-trip (HotbarSlotType::NormalAttack's
    // id is always 0/unused, see HotbarComponent.h), just a display name.
    // Present iff the player currently has a weapon equipped.
    struct NormalAttackEntry
    {
        std::string display_name;
    };

    // The equipped weapon's own elemental prefix special -- same "no id to
    // round-trip" shape as NormalAttackEntry. Present iff the player has a
    // weapon equipped AND it carries an elemental flavor (WeaponComponent::
    // element != Element::None); a non-elemental weapon has no Special
    // Attack to bind.
    struct SpecialAttackEntry
    {
        std::string display_name;
    };

    // Every Technique the player has learned (KnownTechniquesComponent).
    std::vector<TechniqueEntry> techniques;

    // Every Photon Art the currently-equipped weapon grants -- read-only
    // knowledge, not something this screen can teach.
    std::vector<PhotonArtEntry> photon_arts;

    std::optional<NormalAttackEntry> normal_attack;
    std::optional<SpecialAttackEntry> special_attack;
};

} // namespace psr
