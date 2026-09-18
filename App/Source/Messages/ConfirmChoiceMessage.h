#pragma once

namespace psr {

// Published by HudLayer when the player activates the confirm overlay's Yes
// or No row; GameplayLayer's OnConfirmChoice pops ConfirmState and, only if
// confirmed, performs whatever ConfirmState::GetAction() named. Escape while
// the overlay is open is treated as an implicit No, but pops ConfirmState
// directly (see ConfirmState::HandleEvent) without publishing this at all --
// same "cancel needs no message" shape every other modal state's own Escape
// handling already uses.
struct ConfirmChoiceMessage
{
    bool confirmed = false;
};

} // namespace psr
