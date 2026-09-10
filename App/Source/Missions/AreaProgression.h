#pragma once

namespace psr {

struct Dungeon;
class AreaLibrary;
class DungeonLibrary;

// The Dungeon that should follow `current` within its Area's authored
// sequence (Area::dungeon_id_strings, M4.6) -- nullptr if `current`'s
// area_tag resolves to no Area, that Area has no ordered dungeon list,
// `current.id_string` isn't found in it, or `current` is the list's last
// entry. Callers (GameplayLayer's exit-teleporter handling) treat nullptr as
// "return to the hub instead" -- the same fallback IsDungeonUnlocked already
// uses for an area with no authored gating (see RunProgress.h).
const Dungeon* NextDungeonInArea(const Dungeon& current, const AreaLibrary& areas, const DungeonLibrary& dungeons);

} // namespace psr
