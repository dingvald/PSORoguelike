#pragma once

#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <string_view>
#include <utility>

namespace psr {

// Which side of the fight an entity belongs to -- consumed by Hostility.h's
// IsHostile via a fixed relationship table, replacing the old
// player-vs-everyone-else placeholder. Player is only ever assigned
// programmatically (see FactionComponent.h); the other three are authorable.
enum class Faction
{
    Player,
    Ally,
    Enemy,
    Neutral
};

template <> struct EnumNames<Faction>
{
    static constexpr std::array<std::pair<std::string_view, Faction>, 4> kValues{{
        {"player", Faction::Player},
        {"ally", Faction::Ally},
        {"enemy", Faction::Enemy},
        {"neutral", Faction::Neutral},
    }};
};

} // namespace psr
