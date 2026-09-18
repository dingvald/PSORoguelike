#pragma once

#include <string>

namespace psr {

// Published by ConfirmState::OnEnter; HudLayer shows the confirm overlay
// with this prompt text and a Yes/No row list, defaulting focus to No so a
// stray Space/Enter can't accidentally confirm a destructive choice.
struct ConfirmMessage
{
    std::string text;
};

} // namespace psr
