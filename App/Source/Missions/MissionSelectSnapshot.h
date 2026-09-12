#pragma once

#include "Messages/MissionSelectMessage.h"

namespace psr {

class DungeonLibrary;
class AreaLibrary;
struct RunProgress;

// Resolves Mission Select rows: one row per authored Area (pointing at its
// dungeon_id_strings.front() -- selecting "Forest" enters Forest 1, whose
// exit teleporter then walks the rest of the sequence via
// Missions::NextDungeonInArea), plus one row per Dungeon that isn't part of
// any Area's sequence (e.g. a placeholder dungeon authored before an Area
// exists for it). Each row is tagged with IsDungeonUnlocked's result against
// progress.
MissionSelectMessage BuildMissionSelectMessage(const DungeonLibrary& dungeons, const RunProgress& progress,
                                               const AreaLibrary& areas);

} // namespace psr
