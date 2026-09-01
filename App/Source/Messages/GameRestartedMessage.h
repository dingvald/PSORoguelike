#pragma once

namespace psr {

// Published by GameplayLayer once a restart (see RestartRequestedMessage)
// has finished rebuilding the run. HudLayer reacts by hiding the "You Died"
// overlay it showed on PlayerDefeatedMessage and clearing the stale event
// log from the previous run.
struct GameRestartedMessage
{
};

} // namespace psr
