#pragma once

#include "Combat/Faction.h"
#include "Components/AiComponent.h"
#include "Components/FactionComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Engine/ECS/Entity.h"

#include <array>

namespace psr {

// An entity's own FactionComponent if authored; otherwise inferred so
// existing content needs no migration: PlayerControlledComponent -> Player
// (that marker is itself only ever assigned programmatically, never
// authored -- see its own doc comment), AiComponent -> Enemy (every enemy
// prefab today), else Neutral. The Neutral fallback is what closes the old
// placeholder's trap: a hub NPC or prop that later gains a HealthComponent no
// longer becomes attackable by accident just because it isn't the player.
inline Faction GetFaction(Entity entity)
{
    if (const FactionComponent* faction = entity.TryGet<FactionComponent>())
        return faction->faction;
    if (entity.Has<PlayerControlledComponent>())
        return Faction::Player;
    if (entity.Has<AiComponent>())
        return Faction::Enemy;
    return Faction::Neutral;
}

namespace detail {

    // Symmetric relationship table indexed by [Faction][Faction] (declaration
    // order in Faction.h: Player, Ally, Enemy, Neutral). Player and Ally are
    // each hostile only to Enemy; Neutral is hostile to nothing; a faction is
    // never hostile to itself -- Enemy-vs-Enemy is set to false here rather
    // than omitted, so flipping it to true is all real enemy-infighting would
    // take.
    inline constexpr std::array<std::array<bool, 4>, 4> kHostilityTable{{
        /* Player  */ {false, false, true, false},
        /* Ally    */ {false, false, true, false},
        /* Enemy   */ {true, true, false, false},
        /* Neutral */ {false, false, false, false},
    }};

} // namespace detail

inline bool IsHostile(Entity a, Entity b)
{
    return detail::kHostilityTable[static_cast<std::size_t>(GetFaction(a))][static_cast<std::size_t>(GetFaction(b))];
}

} // namespace psr
