#include "Actions/MoveAction.h"

#include "Actions/AttackAction.h"
#include "Combat/Hostility.h"
#include "Components/BlocksMovementComponent.h"
#include "Components/TweenComponent.h"
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
    Position& position = actor.Get<Position>();
    const Vec2 tile = position.tile;
    const Vec2 target = tile + m_offset;

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

        return ActionResult(0, std::make_unique<AttackAction>(*m_grid, *m_affixes, m_offset, *m_rng));
    }
    if (blocked)
        return ActionResult(0);

    m_grid->RemoveEntity(tile, actor.Handle());
    m_grid->AddEntity(target, actor.Handle());
    position.tile = target;

    actor.GetOrEmplace<TweenComponent>() = TweenComponent{
        Vec2f{static_cast<float>(tile.x - target.x), static_cast<float>(tile.y - target.y)}, kMoveTweenDuration, 0.0f};

    return ActionResult(kMoveCost);
}

} // namespace psr
