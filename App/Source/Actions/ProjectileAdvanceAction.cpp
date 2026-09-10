#include "Actions/ProjectileAdvanceAction.h"

#include "Combat/ProjectileImpact.h"
#include "Components/ProjectileComponent.h"
#include "Components/TweenComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Math/Vec2f.h"

namespace psr {

namespace {
    constexpr float kProjectileHopDuration = 0.08f;
} // namespace

ProjectileAdvanceAction::ProjectileAdvanceAction(Grid& grid, const AffixLibrary& affixes, std::mt19937& rng)
    : m_grid(&grid), m_affixes(&affixes), m_rng(&rng)
{
}

ActionResult ProjectileAdvanceAction::Perform(Entity actor)
{
    ProjectileComponent& projectile = actor.Get<ProjectileComponent>();
    Position& position = actor.Get<Position>();

    const Vec2 old_tile = position.tile;
    const Vec2 next_tile = projectile.path[projectile.next_hop];
    ++projectile.next_hop;

    m_grid->RemoveEntity(old_tile, actor.Handle());
    m_grid->AddEntity(next_tile, actor.Handle());
    position.tile = next_tile;

    actor.GetOrEmplace<TweenComponent>().queue.push_back(
        Tween{Vec2f{static_cast<float>(old_tile.x - next_tile.x), static_cast<float>(old_tile.y - next_tile.y)},
              Vec2f{}, kProjectileHopDuration, 0.0f, nullptr});

    const int step_cost = projectile.step_cost;
    Registry& registry = actor.GetRegistry();
    const bool impacted = ResolveProjectileImpact(registry, *m_grid, *m_affixes, *m_rng, projectile, next_tile);

    const bool stopped_by_impact = impacted && !projectile.pierces;
    if (!stopped_by_impact && projectile.next_hop < projectile.path.size())
        return ActionResult(step_cost); // more hops queued for later turns

    registry.DestroyEntity(actor.Handle());
    return ActionResult(step_cost);
}

} // namespace psr
