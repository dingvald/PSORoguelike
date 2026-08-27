#include "Actions/AttackAction.h"

#include "Combat/EffectiveStats.h"
#include "Combat/Hostility.h"
#include "Components/BlocksMovementComponent.h"
#include "Components/EquipmentComponent.h"
#include "Engine/Combat/CombatMath.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/RaceComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/StatsComponent.h"
#include "Engine/ECS/WeaponComponent.h"

#include <algorithm>
#include <vector>

namespace psr {

namespace {
    // A tile blocks the attack line but is never itself a target: something
    // occupies it that blocks movement and carries no HealthComponent (a
    // wall/obstacle, as opposed to a hostile actor -- see MoveAction's own
    // BlocksMovementComponent check for the movement-side equivalent).
    bool IsWallTile(Registry& registry, const Grid& grid, Vec2 tile)
    {
        for (entt::entity occupant : grid.GetEntities(tile))
            if (registry.HasComponent<BlocksMovementComponent>(occupant) &&
                !registry.HasComponent<HealthComponent>(occupant))
                return true;
        return false;
    }

    std::vector<Vec2> ResolveTargetTiles(const Grid& grid, Registry& registry, Vec2 origin, Vec2 direction,
                                         const WeaponComponent& weapon)
    {
        std::vector<Vec2> tiles;
        switch (weapon.range_shape)
        {
        case WeaponRangeShape::SingleTarget:
        {
            const Vec2 tile = origin + direction;
            if (grid.Contains(tile))
                tiles.push_back(tile);
            break;
        }
        case WeaponRangeShape::Line:
        {
            for (int i = 1; i <= weapon.range; ++i)
            {
                const Vec2 tile = origin + direction * i;
                if (!grid.Contains(tile))
                    break;
                if (IsWallTile(registry, grid, tile))
                    break;
                tiles.push_back(tile);
            }
            break;
        }
        case WeaponRangeShape::Cone3:
        {
            const Vec2 perpendicular{-direction.y, direction.x};
            for (Vec2 tile :
                 {origin + direction, origin + direction + perpendicular, origin + direction - perpendicular})
                if (grid.Contains(tile))
                    tiles.push_back(tile);
            break;
        }
        case WeaponRangeShape::Surrounding:
        {
            for (Vec2 offset : {Vec2{1, 0}, Vec2{-1, 0}, Vec2{0, 1}, Vec2{0, -1}})
            {
                const Vec2 tile = origin + offset;
                if (grid.Contains(tile))
                    tiles.push_back(tile);
            }
            break;
        }
        }
        return tiles;
    }
} // namespace

AttackAction::AttackAction(Grid& grid, const AffixLibrary& affixes, Vec2 direction, std::mt19937& rng)
    : m_grid(&grid), m_affixes(&affixes), m_direction(direction), m_rng(&rng)
{
}

ActionResult AttackAction::Perform(Entity actor)
{
    const EquipmentComponent* equipment = actor.TryGet<EquipmentComponent>();
    if (!equipment || equipment->weapon == entt::null)
        return ActionResult(0);

    Registry& registry = actor.GetRegistry();
    const WeaponComponent* weapon = registry.TryGetComponent<WeaponComponent>(equipment->weapon);
    if (!weapon)
        return ActionResult(0);

    const Vec2 origin = actor.Get<Position>().tile;
    const std::vector<Vec2> target_tiles = ResolveTargetTiles(*m_grid, registry, origin, m_direction, *weapon);

    const StatsComponent attacker_stats = ComputeEffectiveStats(actor, *m_affixes);
    std::uniform_real_distribution<float> unit_roll(0.0f, 1.0f);
    std::uniform_real_distribution<float> variance_roll(0.9f, 1.1f);

    bool found_target = false;
    for (Vec2 tile : target_tiles)
    {
        // Snapshot the occupant list before hitting anything -- a lethal hit
        // mutates the Grid's own occupant vector via RemoveEntity, which
        // would otherwise invalidate iteration.
        const std::vector<entt::entity> occupants = m_grid->GetEntities(tile);
        for (entt::entity occupant : occupants)
        {
            if (occupant == actor.Handle() || !registry.HasComponent<HealthComponent>(occupant))
                continue;
            Entity target(registry, occupant);
            if (!IsHostile(actor, target))
                continue;

            found_target = true;
            const StatsComponent defender_stats = ComputeEffectiveStats(target, *m_affixes);
            const RaceComponent* defender_race = target.TryGet<RaceComponent>();
            const std::uint32_t defender_race_id = defender_race ? defender_race->race_id : 0;

            for (int hit = 0; hit < weapon->hits_per_turn; ++hit)
            {
                if (!target.IsValid())
                    break;

                const float hit_chance = ComputeHitChance(attacker_stats.ata, defender_stats.evp);
                if (unit_roll(*m_rng) > hit_chance)
                    continue; // miss

                int damage = ComputeDamage(attacker_stats.atp, defender_stats.dfp, variance_roll(*m_rng));
                damage = ApplyRaceBonus(damage, weapon->race_bonuses, defender_race_id);

                HealthComponent& health = target.Get<HealthComponent>();
                health.current_hp = std::max(0, health.current_hp - damage);
                if (health.current_hp == 0)
                {
                    m_grid->RemoveEntity(tile, occupant);
                    registry.DestroyEntity(occupant);
                    break;
                }
            }
        }
    }

    return found_target ? ActionResult(kAttackCost) : ActionResult(0);
}

} // namespace psr
