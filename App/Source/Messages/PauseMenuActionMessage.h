#pragma once

namespace psr {

// The three pause-menu rows that actually change engine state -- Options/
// Help are inert placeholders HudLayer resolves entirely locally (same
// "Coming Soon" precedent MainMenuLayer's own Options/Credits rows set), so
// they never publish this at all.
enum class PauseMenuAction
{
    Resume,
    QuitToTitle,
    QuitToDesktop
};

// Published by HudLayer when the player activates a Resume/Quit to Title/
// Quit to Desktop row on the pause menu; GameplayLayer's OnPauseMenuAction
// resolves it (Resume pops PauseState directly, both Quit rows route through
// ConfirmState first -- see its own doc comment).
struct PauseMenuActionMessage
{
    PauseMenuAction action;
};

} // namespace psr
