#pragma once

#include <string>

namespace psr {

// Published by GameplayLayer::OnTeleporterActivated when the player activates
// a dungeon's Exit teleporter (TeleporterDestination::AdvanceLevel) -- once
// per dungeon completed, whether that advances to the next level in the same
// Area or ends the mission back at the hub. HudLayer logs it via the existing
// event-log path -- no new UI chrome.
struct MissionCompletedMessage
{
    std::string dungeon_id_string;
};

} // namespace psr
