#pragma once

#include <cstdint>
#include <unordered_set>

namespace psr {

struct Dungeon;
class DungeonLibrary;
class AreaLibrary;

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
// with `progress`. M4.5's fixed area unlock order: `dungeon`'s Area (looked
// up via its area_tag in `areas`) is unlocked unconditionally if it has no
// authored unlock_predecessor_tag; otherwise it requires at least one dungeon
// tagged with that predecessor area to already be in
// progress.completed_dungeon_ids. An area with no Area entry authored yet
// (areas.FindByTag finds nothing -- e.g. today's placeholder test_dungeon,
// whose area_tag is empty) stays unconditionally unlocked, same as this
// function's pre-M4.5 placeholder policy -- this only tightens gating once
// an author actually opts an area into it. M10.2 is expected to add a tier
// parameter once difficulty tiers exist, so "finishing a mission at a given
// tier unlocks the next tier up for the same area" (per docs/GDD.md) can
// actually be expressed.
bool IsDungeonUnlocked(const RunProgress& progress, const Dungeon& dungeon, const DungeonLibrary& dungeons,
                       const AreaLibrary& areas);

} // namespace psr
