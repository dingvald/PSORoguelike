#include "Systems/EnemyAiSystem.h"

#include "Actions/MoveAction.h"
#include "Actions/TechniqueAction.h"
#include "Combat/Hostility.h"
#include "Components/BlocksMovementComponent.h"
#include "Components/PackFollowerComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RaceComponent.h"
#include "Components/RangedTechComponent.h"
#include "Components/SelectedTargetComponent.h"
#include "Components/SpawnedByComponent.h"
#include "Components/SpawnerAiComponent.h"
#include "Components/StatsComponent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/Math/Vec2.h"

#include <array>
#include <cstdlib>
#include <optional>

namespace psr {

namespace {

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

EnemyAiSystem::EnemyAiSystem(Grid& grid, Registry& registry, const AffixLibrary& affixes,
                             const TechniqueLibrary& techniques, std::mt19937& rng,
                             std::function<void(entt::entity)> on_spawned)
    : m_grid(&grid), m_registry(&registry), m_affixes(&affixes), m_techniques(&techniques), m_rng(&rng),
      m_on_spawned(std::move(on_spawned))
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
    case AiBehavior::FleeWhenHit:
        action = DecideFleeWhenHit(actor, *ai);
        break;
    case AiBehavior::StationarySpawner:
        action = DecideStationarySpawner(actor);
        break;
    case AiBehavior::PackFollower:
        action = DecidePackFollower(actor, *ai);
        break;
    case AiBehavior::RangedTechAtDistance:
        action = DecideRangedTechAtDistance(actor, *ai);
        break;
    }
    return action ? action : &m_wait_action;
}

IAction* EnemyAiSystem::StepToward(Entity actor, Vec2 self_tile, Vec2 delta)
{
    const Vec2 diagonal{delta.x > 0 ? 1 : (delta.x < 0 ? -1 : 0), delta.y > 0 ? 1 : (delta.y < 0 ? -1 : 0)};

    if (IsViableStep(*m_grid, *m_registry, actor, self_tile + diagonal))
    {
        m_pending_decision = std::make_unique<MoveAction>(*m_grid, *m_affixes, diagonal, *m_rng);
        return m_pending_decision.get();
    }

    // Diagonal step blocked (a wall corner, typically) -- degrade to a single-
    // axis step along whichever axis has the larger delta, then the other, so
    // a corridor that blocks the exact diagonal doesn't stall the chase every
    // single turn.
    const bool x_dominant = std::abs(delta.x) >= std::abs(delta.y);
    const Vec2 first_axis = x_dominant ? Vec2{diagonal.x, 0} : Vec2{0, diagonal.y};
    const Vec2 second_axis = x_dominant ? Vec2{0, diagonal.y} : Vec2{diagonal.x, 0};

    if (first_axis != Vec2{0, 0} && IsViableStep(*m_grid, *m_registry, actor, self_tile + first_axis))
    {
        m_pending_decision = std::make_unique<MoveAction>(*m_grid, *m_affixes, first_axis, *m_rng);
        return m_pending_decision.get();
    }

    if (second_axis != Vec2{0, 0} && IsViableStep(*m_grid, *m_registry, actor, self_tile + second_axis))
    {
        m_pending_decision = std::make_unique<MoveAction>(*m_grid, *m_affixes, second_axis, *m_rng);
        return m_pending_decision.get();
    }

    return nullptr;
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

    return StepToward(actor, self_position->tile, *target_tile - self_position->tile);
}

IAction* EnemyAiSystem::DecideFleeWhenHit(Entity actor, const AiComponent& ai)
{
    const Position* self_position = actor.TryGet<Position>();
    if (!self_position)
        return nullptr;

    const std::optional<Vec2> target_tile =
        FindNearestHostileTile(*m_registry, actor, self_position->tile, ai.detection_range);
    if (!target_tile)
        return nullptr;

    const HealthComponent* health = actor.TryGet<HealthComponent>();
    const bool has_been_hit = health && health->current_hp < health->max_hp;
    const Vec2 delta = *target_tile - self_position->tile;

    return StepToward(actor, self_position->tile, has_been_hit ? -delta : delta);
}

IAction* EnemyAiSystem::DecideStationarySpawner(Entity actor)
{
    SpawnerAiComponent* spawner = actor.TryGet<SpawnerAiComponent>();
    if (!spawner)
        return nullptr;

    if (spawner->cooldown_remaining > 0)
    {
        --spawner->cooldown_remaining;
        return nullptr;
    }

    const Position* self_position = actor.TryGet<Position>();
    if (!self_position || !m_registry->HasPrefab(spawner->spawn_prefab_id))
        return nullptr;

    int alive_count = 0;
    m_registry->Each<SpawnedByComponent>(
        [&](entt::entity /*candidate*/, const SpawnedByComponent& spawned_by)
        {
            if (spawned_by.owner == actor.Handle())
                ++alive_count;
        });
    if (alive_count >= spawner->max_alive)
        return nullptr;

    static constexpr std::array<Vec2, 4> kCardinalOffsets = {Vec2{0, -1}, Vec2{0, 1}, Vec2{-1, 0}, Vec2{1, 0}};
    Vec2 spawn_tile = self_position->tile;
    for (Vec2 offset : kCardinalOffsets)
    {
        const Vec2 candidate_tile = self_position->tile + offset;
        if (IsViableStep(*m_grid, *m_registry, actor, candidate_tile))
        {
            spawn_tile = candidate_tile;
            break;
        }
    }

    const entt::entity spawned = m_registry->CreateEntity(spawner->spawn_prefab_id);
    m_registry->Emplace<Position>(spawned, Position{spawn_tile});
    m_registry->Emplace<SpawnedByComponent>(spawned, SpawnedByComponent{actor.Handle()});
    m_grid->AddEntity(spawn_tile, spawned);
    if (m_on_spawned)
        m_on_spawned(spawned);

    spawner->cooldown_remaining = spawner->cooldown_turns;
    return nullptr;
}

IAction* EnemyAiSystem::DecidePackFollower(Entity actor, const AiComponent& ai)
{
    PackFollowerComponent* pack = actor.TryGet<PackFollowerComponent>();
    const Position* self_position = actor.TryGet<Position>();

    if (pack && self_position && !pack->has_panicked)
    {
        bool leader_nearby = false;
        m_registry->Each<RaceComponent>(
            [&](entt::entity candidate, const RaceComponent& race)
            {
                if (leader_nearby || candidate == actor.Handle())
                    return;
                if (race.race_id != pack->pack_leader_race_id)
                    return;

                const Position* candidate_position = m_registry->TryGetComponent<Position>(candidate);
                if (candidate_position &&
                    ManhattanDistance(self_position->tile, candidate_position->tile) <= ai.detection_range)
                    leader_nearby = true;
            });

        if (!leader_nearby)
        {
            pack->has_panicked = true;
            if (StatsComponent* stats = actor.TryGet<StatsComponent>())
            {
                // Integer math (x7/10), not a float multiply -- 0.7f isn't
                // exactly representable, so a truncating cast after a float
                // multiply can silently under-count by one (e.g. 100 * 0.7f
                // truncates to 69, not 70).
                stats->atp = stats->atp * 7 / 10;
                stats->dfp = stats->dfp * 7 / 10;
            }
        }
    }

    return DecideChaseAndAttack(actor, ai);
}

IAction* EnemyAiSystem::DecideRangedTechAtDistance(Entity actor, const AiComponent& ai)
{
    const RangedTechComponent* ranged_tech = actor.TryGet<RangedTechComponent>();
    if (!ranged_tech)
        return DecideChaseAndAttack(actor, ai);

    const Position* self_position = actor.TryGet<Position>();
    if (!self_position)
        return nullptr;

    const std::optional<Vec2> target_tile =
        FindNearestHostileTile(*m_registry, actor, self_position->tile, ai.detection_range);
    if (!target_tile)
        return nullptr;

    const Vec2 delta = *target_tile - self_position->tile;
    const int distance = ManhattanDistance(self_position->tile, *target_tile);
    const bool aligned = delta.x == 0 || delta.y == 0;

    if (distance > 1 && aligned && distance <= ranged_tech->range)
    {
        actor.GetOrEmplace<SelectedTargetComponent>().tile = *target_tile;
        m_pending_decision =
            std::make_unique<TechniqueAction>(*m_grid, *m_techniques, *m_affixes, ranged_tech->technique_id, *m_rng);
        return m_pending_decision.get();
    }

    return StepToward(actor, self_position->tile, delta);
}

} // namespace psr
