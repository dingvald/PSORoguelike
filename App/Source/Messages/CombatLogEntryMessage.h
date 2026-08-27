#pragma once

#include <string>

namespace psr {

// One already-formatted line for HudLayer's event log, published by
// CombatLogBridge. Text is fully resolved (display names, not entity
// handles/prefab ids) so HudLayer never needs ECS access to show it.
struct CombatLogEntryMessage
{
    std::string text;
};

} // namespace psr
