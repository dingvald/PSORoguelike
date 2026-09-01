#include "Systems/EnemyAiSystem.h"

#include "Actions/MoveAction.h"
#include "Combat/Hostility.h"
#include "Combat/TargetResolution.h"
#include "Components/BlocksMovementComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/Math/Vec2.h"

#include <cstdlib>
#include <optional>

namespace psr {

namespace {
    int ManhattanDistance(Vec2 a, Vec2 b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }

    // Nearest PlayerControlledComponent entity's tile within detection_range,
    // by Manhattan distance -- mirrors Hostility.h's own player-vs-everyone-
    // else placeholder rather than a general hostile-faction query, since
    // that's the only opposing faction that exists today.
    std::optional<Vec2> FindNearestHostileTile(Registry& registry, Entity actor, Vec2 self_tile, int detection_range)
    {
        std::optional<Vec2> best_tile;
        int best_distance = detection_range + 1;

        registry.Each<PlayerControlledComponent>(
            [&](entt::entity candidate)
            {
                Entity target(registry, candidate);
                if (!IsHostile(actor, target))
                    return;

                const Position* position = target.TryGet<Position>();
                if (!position)
                    return;

                const int distance = ManhattanDistance(self_tile, position->tile);
                if (distance <= detection_range && distance < best_distance)
                {
                    best_distance = distance;
                    best_tile = position->tile;
                }
            });

        return best_tile;
    }

    // Same occupancy check MoveAction::Perform itself does: a tile is a
    // viable step if it's empty of BlocksMovementComponent occupants, or its
    // (sole expected) blocking occupant is a hostile with HealthComponent --
    // MoveAction's own bump fallback is what turns stepping there into an
    // AttackAction.
    bool IsViableStep(Grid& grid, Registry& registry, Entity actor, Vec2 target_tile)
    {
        if (!grid.Contains(target_tile))
            return false;

        bool blocked = false;
        for (entt::entity occupant : grid.GetEntities(target_tile))
        {
            if (!registry.HasComponent<BlocksMovementComponent>(occupant))
                continue;
            blocked = true;

            if (!registry.HasComponent<HealthComponent>(occupant))
                continue;
            if (IsHostile(actor, Entity(registry, occupant)))
                return true;
        }
        return !blocked;
    }
} // namespace

EnemyAiSystem::EnemyAiSystem(Grid& grid, Registry& registry, const AffixLibrary& affixes, std::mt19937& rng)
    : m_grid(&grid), m_registry(&registry), m_affixes(&affixes), m_rng(&rng)
{
}

IAction* EnemyAiSystem::Decide(Entity actor)
{
    const AiComponent* ai = actor.TryGet<AiComponent>();
    if (!ai)
        return &m_wait_action;

    IAction* action = nullptr;
    switch (ai->behavior)
    {
    case AiBehavior::ChaseAndAttack:
        action = DecideChaseAndAttack(actor, *ai);
        break;
    }
    return action ? action : &m_wait_action;
}

IAction* EnemyAiSystem::DecideChaseAndAttack(Entity actor, const AiComponent& ai)
{
    const Position* self_position = actor.TryGet<Position>();
    if (!self_position)
        return nullptr;

    const std::optional<Vec2> target_tile =
        FindNearestHostileTile(*m_registry, actor, self_position->tile, ai.detection_range);
    if (!target_tile)
        return nullptr;

    const Vec2 delta = *target_tile - self_position->tile;
    const Vec2 primary = SnapToCardinalDirection(delta);

    if (IsViableStep(*m_grid, *m_registry, actor, self_position->tile + primary))
    {
        m_pending_decision = std::make_unique<MoveAction>(*m_grid, *m_affixes, primary, *m_rng);
        return m_pending_decision.get();
    }

    // Primary axis blocked (a wall, typically) -- try the other axis before
    // giving up, so a straight corridor perpendicular to the target doesn't
    // stall the chase every single turn.
    if (delta.x != 0 && delta.y != 0)
    {
        const Vec2 secondary = primary.x != 0 ? Vec2{0, delta.y > 0 ? 1 : -1} : Vec2{delta.x > 0 ? 1 : -1, 0};
        if (IsViableStep(*m_grid, *m_registry, actor, self_position->tile + secondary))
        {
            m_pending_decision = std::make_unique<MoveAction>(*m_grid, *m_affixes, secondary, *m_rng);
            return m_pending_decision.get();
        }
    }

    return nullptr;
}

} // namespace psr
