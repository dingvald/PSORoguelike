#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <array>
#include <cstdint>

namespace psr {

enum class HotbarSlotType
{
    Empty,
    Technique,
    PhotonArt,
    Item
};

// One quick-use slot: type plus a NameId into TechniqueLibrary/PhotonArtLibrary
// (unused for Item -- there is no item id space yet, see ItemPickupEvent.h).
struct HotbarSlot
{
    HotbarSlotType type = HotbarSlotType::Empty;
    std::uint32_t id = 0;
};

// The player's 10 quick-use slots (keys 1-9, 0). Runtime-only player state,
// populated programmatically in GameplayLayer::OnAttach -- not authorable,
// same treatment as PlayerControlledComponent/PrefabIdComponent, since there
// is no loadout-authoring flow to hand this to content yet.
struct HotbarComponent
{
    static constexpr std::size_t kSlotCount = 10;
    std::array<HotbarSlot, kSlotCount> slots{};

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<HotbarComponent>("hotbar", /*authorable=*/false);
    }
};

} // namespace psr
