#pragma once

#include "Messages/MissionSelectMessage.h"

namespace psr {

class DungeonLibrary;
class AreaLibrary;
struct RunProgress;

// Resolves every authored Dungeon into a Mission Select row, tagging each
// with IsDungeonUnlocked's result against progress (areas resolves each
// dungeon's area_tag for M4.5's unlock-order check).
MissionSelectMessage BuildMissionSelectMessage(const DungeonLibrary& dungeons, const RunProgress& progress,
                                               const AreaLibrary& areas);

} // namespace psr
