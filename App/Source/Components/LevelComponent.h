#pragma once

namespace psr {

// The player's current level and XP banked toward the next one -- emplaced
// on the player at spawn (GameplayLayer) and maintained by ExperienceSystem
// from there, never hand-authored on a prefab (same treatment as
// TabTargetComponent), so this is deliberately not schema-registered. xp is
// progress toward level+1, not a lifetime total -- ExperienceSystem consumes
// it (and carries any remainder forward) on each level-up rather than
// tracking a running grand total.
struct LevelComponent
{
    int level = 1;
    int xp = 0;
};

} // namespace psr
