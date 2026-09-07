#pragma once

#include "Messages/MissionSelectMessage.h"

namespace psr {

class DungeonLibrary;
struct RunProgress;

// Resolves every authored Dungeon into a Mission Select row, tagging each
// with IsDungeonUnlocked's result against progress.
MissionSelectMessage BuildMissionSelectMessage(const DungeonLibrary& dungeons, const RunProgress& progress);

} // namespace psr
