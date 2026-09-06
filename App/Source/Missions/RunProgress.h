#pragma once

#include <cstdint>
#include <unordered_set>

namespace psr {

struct Dungeon;

// In-memory, session-scoped, per-process progress toward mission unlocks --
// no serialization exists yet (M11.2 "run persistence & permadeath" hasn't
// started; see docs/ROADMAP.md), same "wire the seam, defer the storage"
// deferral this project already used for M7.2's grind_level/M5.2's
// spawn-weight. A real per-character save replaces this struct's storage,
// not IsDungeonUnlocked's signature. Never reset by a Hub<->Dungeon scene
// swap -- only a future new-character flow (M10.3/M11.2) would reset it.
struct RunProgress
{
    std::unordered_set<std::uint32_t> completed_dungeon_ids; // Dungeon::id values
};

// Whether `dungeon` should be selectable from Mission Select for a character
// with `progress`. Placeholder policy this milestone: every authored Dungeon
// is unlocked unconditionally -- there is no fixed area order yet (M4.5,
// "Forest->Caves->Mines->Ruins gating hook, consumed by the hub in M10," is
// separately not started) and no difficulty tiers yet (M10.2). M4.5 is
// expected to extend this function's body (not signature) to also require
// dungeon.area_tag's predecessor area to already be in
// progress.completed_dungeon_ids; M10.2 is expected to add a tier parameter
// once tiers exist, so "finishing a mission at a given tier unlocks the next
// tier up for the same area" (per docs/GDD.md) can actually be expressed.
bool IsDungeonUnlocked(const RunProgress& progress, const Dungeon& dungeon);

} // namespace psr
