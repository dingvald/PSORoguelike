#pragma once

#include "Engine/Actions/IAction.h"
#include "Engine/Math/Vec2.h"
#include "Engine/World/Grid.h"

namespace psr {

// Moves actor by a fixed tile offset. A target tile outside the Grid, or
// occupied by a BlocksMovementComponent entity, is a free no-op (cost 0, no
// turn consumed) -- there's no bump-to-attack fallback yet, since combat
// (M7) doesn't exist. On success, Position snaps to the target tile
// immediately (so turn logic never waits on animation) and a TweenComponent
// is emplaced so the render offset eases in from the old tile.
class MoveAction : public IAction
{
public:
    static constexpr int kMoveCost = 100;
    static constexpr float kMoveTweenDuration = 0.12f;

    MoveAction(Grid& grid, Vec2 offset);

    ActionResult Perform(Entity actor) override;

private:
    Grid* m_grid;
    Vec2 m_offset;
};

} // namespace psr
