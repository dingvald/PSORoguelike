#include "Actions/MoveAction.h"

#include "Components/BlocksMovementComponent.h"
#include "Components/TweenComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/Math/Vec2f.h"

namespace psr {

MoveAction::MoveAction(Grid& grid, Vec2 offset) : m_grid(&grid), m_offset(offset) {}

ActionResult MoveAction::Perform(Entity actor)
{
    Position& position = actor.Get<Position>();
    const Vec2 tile = position.tile;
    const Vec2 target = tile + m_offset;

    if (!m_grid->Contains(target))
        return ActionResult(0);

    Registry& registry = actor.GetRegistry();
    for (entt::entity occupant : m_grid->GetEntities(target))
    {
        if (registry.HasComponent<BlocksMovementComponent>(occupant))
            return ActionResult(0);
    }

    m_grid->RemoveEntity(tile, actor.Handle());
    m_grid->AddEntity(target, actor.Handle());
    position.tile = target;

    actor.GetOrEmplace<TweenComponent>() =
        TweenComponent{Vec2f{static_cast<float>(tile.x - target.x), static_cast<float>(tile.y - target.y)},
                       kMoveTweenDuration, 0.0f};

    return ActionResult(kMoveCost);
}

} // namespace psr
