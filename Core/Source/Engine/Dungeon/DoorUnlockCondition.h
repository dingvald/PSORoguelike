#pragma once

#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <string_view>
#include <utility>

namespace psr {

// How a locked door placed by DungeonStitcher's Phase 4 (see DungeonStitcher.cpp)
// unlocks -- authored per-DungeonPiece (DungeonPiece::preferred_unlock_condition) on
// whichever piece ends up gated behind the lock, not on the lock/Dungeon itself, so a
// Vault piece always wants a switch puzzle and a BossArena always wants room-clear
// regardless of which Dungeon places it. Switch: a single switch entity, placed by the
// stitcher in some other already-reachable room (see LockAnnotation::switch_cell),
// unlocks the door when triggered (see SwitchTriggerSystem). RoomCleared: the door
// unlocks once every entity spawned into its own gated room has died (see
// RoomClearDoorSystem).
enum class DoorUnlockCondition
{
    Switch,
    RoomCleared
};

template <> struct EnumNames<DoorUnlockCondition>
{
    static constexpr std::array<std::pair<std::string_view, DoorUnlockCondition>, 2> kValues{{
        {"switch", DoorUnlockCondition::Switch},
        {"room_cleared", DoorUnlockCondition::RoomCleared},
    }};
};

} // namespace psr
