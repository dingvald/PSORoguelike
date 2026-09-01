#include "Actions/MoveAction.h"

#include "Actions/AttackAction.h"
#include "Combat/Hostility.h"
#include "Components/BlocksMovementComponent.h"
#include "Components/TweenComponent.h"
#include "Engine/Actions/MoveEvent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/Math/Vec2f.h"

#include <memory>

namespace psr {

MoveAction::MoveAction(Grid& grid, const AffixLibrary& affixes, Vec2 offset, std::mt19937& rng)
    : m_grid(&grid), m_affixes(&affixes), m_offset(offset), m_rng(&rng)
{
}

ActionResult MoveAction::Perform(Entity actor)
{
    // StatusEffectComponent's own handler reacts to this: a Confuse stack
    // overwrites offset with a random cardinal direction before it's read
    // back below (mutable, same "read back after dispatch" contract
    // BeforeDamageEvent::incoming_damage already establishes); a
    // root/immobilize-style effect could instead veto the move via
    // cancelled.
    BeforeMoveEvent before_move{m_offset};
    actor.Dispatch(before_move);
    if (before_move.cancelled)
        return ActionResult(0);
    const Vec2 offset = before_move.offset; // may have been redirected above

    Position& position = actor.Get<Position>();
    const Vec2 tile = position.tile;
    const Vec2 target = tile + offset;

    if (!m_grid->Contains(target))
        return ActionResult(0);

    Registry& registry = actor.GetRegistry();
    bool blocked = false;
    for (entt::entity occupant : m_grid->GetEntities(target))
    {
        if (!registry.HasComponent<BlocksMovementComponent>(occupant))
            continue;
        blocked = true;

        if (!registry.HasComponent<HealthComponent>(occupant))
            continue;
        if (!IsHostile(actor, Entity(registry, occupant)))
            continue;

        return ActionResult(0, std::make_unique<AttackAction>(*m_grid, *m_affixes, offset, *m_rng));
    }
    if (blocked)
        return ActionResult(0);

    m_grid->RemoveEntity(tile, actor.Handle());
    m_grid->AddEntity(target, actor.Handle());
    position.tile = target;

    actor.GetOrEmplace<TweenComponent>().queue.push_back(
        Tween{Vec2f{static_cast<float>(tile.x - target.x), static_cast<float>(tile.y - target.y)}, Vec2f{},
              kMoveTweenDuration, 0.0f, nullptr});

    AfterMoveEvent after_move{tile, target};
    actor.Dispatch(after_move);

    return ActionResult(kMoveCost);
}

} // namespace psr
