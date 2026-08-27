#pragma once

#include "Components/HotbarComponent.h"

#include <array>
#include <string>

namespace psr {

// Resolved hotbar contents for HudLayer to render -- a display name per
// slot, not an id, so HudLayer never needs a TechniqueLibrary/PhotonArtLibrary
// reference to show it. Published once by GameplayLayer right after it
// builds the player's HotbarComponent (hotbar contents don't change
// dynamically yet -- there is no re-equip flow -- so one publish per
// GameplayLayer::OnAttach is sufficient for now).
struct HotbarStateMessage
{
    struct SlotView
    {
        HotbarSlotType type = HotbarSlotType::Empty;
        std::string name;
    };

    std::array<SlotView, HotbarComponent::kSlotCount> slots{};
};

} // namespace psr
