#pragma once

namespace psr {

// Published by PauseState::OnEnter; HudLayer shows the pause overlay in
// response. No payload -- the row set (Resume/Options/Help/Quit to Title/
// Quit to Desktop) is fixed engine UI, not authored content, so it's baked
// directly into hud.rml rather than resolved into an Entry list the way
// MissionSelectMessage's rows are.
struct PauseMenuMessage
{
};

} // namespace psr
