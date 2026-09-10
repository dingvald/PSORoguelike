#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <string_view>
#include <utility>

namespace psr {

// Where standing on this entity's tile and pressing Space sends the player
// (see GameplayLayer's dungeon-scene Space handling and
// Missions/TeleporterInteraction.h's FindTeleporterAt) -- a dungeon-scene
// structural gameplay concept, same fixed-enum reasoning InteractionType's
// own doc comment gives.
enum class TeleporterDestination
{
    ReturnToHub,
    AdvanceLevel
};

template <> struct EnumNames<TeleporterDestination>
{
    static constexpr std::array<std::pair<std::string_view, TeleporterDestination>, 2> kValues{{
        {"return_to_hub", TeleporterDestination::ReturnToHub},
        {"advance_level", TeleporterDestination::AdvanceLevel},
    }};
};

// Marks a placed dungeon entity (an Entrance/Exit piece's stamped teleporter)
// as something the player can activate by walking onto its tile and pressing
// Space -- see Missions/TeleporterInteraction.h's FindTeleporterAt.
// ReturnToHub bails out of the mission with no completion credit (same as the
// 'H' abandon key); AdvanceLevel credits the current dungeon as completed and
// steps into the next dungeon in its Area's sequence, or the hub if this was
// the last one (see Missions/AreaProgression.h's NextDungeonInArea).
struct TeleporterComponent
{
    TeleporterDestination destination = TeleporterDestination::ReturnToHub;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<TeleporterComponent>("teleporter").Data<&TeleporterComponent::destination>("destination");
    }
};

} // namespace psr
