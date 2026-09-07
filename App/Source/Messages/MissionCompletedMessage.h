#pragma once

#include <string>

namespace psr {

// Published by GameplayLayer::OnMissionExitReached once the player steps
// into a dungeon's Exit piece and the scene swaps back to the hub.
// HudLayer logs it via the existing event-log path -- no new UI chrome.
struct MissionCompletedMessage
{
    std::string dungeon_id_string;
};

} // namespace psr
