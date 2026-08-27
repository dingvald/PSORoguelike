#pragma once

#include "Engine/Actions/IAction.h"
#include "Engine/Items/AffixLibrary.h"
#include "Engine/Math/Vec2.h"
#include "Engine/World/Grid.h"

#include <random>

namespace psr {

// Moves actor by a fixed tile offset. A target tile outside the Grid is a
// free no-op (cost 0, no turn consumed). A target tile occupied by a
// BlocksMovementComponent entity is either: a free no-op, if nothing
// attackable/hostile is there (a wall/obstacle); or a bump-to-attack --
// returned as an AttackAction fallback (see ActionResult::fallback,
// ResolveAction) rather than performed here, if the occupant has a
// HealthComponent and is hostile (see Hostility.h). On success, Position
// snaps to the target tile immediately (so turn logic never waits on
// animation) and a TweenComponent is emplaced so the render offset eases in
// from the old tile.
class MoveAction : public IAction
{
public:
    static constexpr int kMoveCost = 100;
    static constexpr float kMoveTweenDuration = 0.12f;

    MoveAction(Grid& grid, const AffixLibrary& affixes, Vec2 offset, std::mt19937& rng);

    ActionResult Perform(Entity actor) override;

private:
    Grid* m_grid;
    const AffixLibrary* m_affixes;
    Vec2 m_offset;
    std::mt19937* m_rng;
};

} // namespace psr
