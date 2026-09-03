#include "Actions/ProjectileAdvanceAction.h"

#include "Combat/CombatMath.h"
#include "Combat/EffectFamily.h"
#include "Combat/EffectiveStats.h"
#include "Combat/Hostility.h"
#include "Combat/StatusEffectApplication.h"
#include "Combat/StatusEffectHooks.h"
#include "Components/ElementalResistanceComponent.h"
#include "Components/ProjectileComponent.h"
#include "Components/StatsComponent.h"
#include "Components/TweenComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Math/Vec2f.h"

#include <cmath>
#include <random>
#include <vector>

namespace psr {

namespace {
    constexpr float kProjectileHopDuration = 0.08f;

    void ResolveImpact(Registry& registry, const Grid& grid, const AffixLibrary& affixes, std::mt19937& rng,
                       const ProjectileComponent& projectile, const std::vector<Vec2>& impact_tiles)
    {
        if (!registry.IsValid(projectile.source))
            return; // caster died mid-flight -- nothing to attribute the hit to

        Entity source(registry, projectile.source);
        std::uniform_real_distribution<float> unit_roll(0.0f, 1.0f);

        for (Vec2 tile : impact_tiles)
        {
            // Snapshot before hitting anything -- a lethal hit mutates the
            // Grid's own occupant vector via RemoveEntity, same precaution
            // every other Action's hit loop takes.
            const std::vector<entt::entity> occupants = grid.GetEntities(tile);
            for (entt::entity occupant : occupants)
            {
                if (occupant == projectile.source || !registry.HasComponent<HealthComponent>(occupant))
                    continue;
                Entity target(registry, occupant);
                if (!IsHostile(source, target))
                    continue;

                const StatsComponent defender_stats = ComputeEffectiveStats(target, affixes);
                const float hit_chance = ComputeHitChance(projectile.attacker_stats.ata, defender_stats.evp);
                if (unit_roll(rng) > hit_chance)
                {
                    AttackMissEvent miss{target};
                    source.Dispatch(miss);
                    continue;
                }

                if (projectile.effect_family == EffectFamily::Status)
                {
                    ApplyStatusEffect(target, registry.GetStatusEffectLibrary(), projectile.status_effect_id);
                    continue;
                }

                const ElementalResistanceComponent* defender_resistance = target.TryGet<ElementalResistanceComponent>();
                const int resistance_percent =
                    defender_resistance ? defender_resistance->ResistanceFor(projectile.element) : 0;
                int damage = static_cast<int>(
                    std::lround(ComputeTechniqueDamage(projectile.attacker_stats.mst, resistance_percent) *
                                projectile.power_multiplier));

                BeforeDamageEvent before{target, damage};
                source.Dispatch(before);
                damage = before.incoming_damage;

                IncomingDamageEvent incoming{source, damage, false, projectile.hit_effect_prefab_id,
                                             projectile.hit_effect_duration};
                target.Dispatch(incoming);

                if (!target.IsValid())
                    continue;

                MaybeApplyElementalStatus(target, registry.GetStatusEffectLibrary(), projectile.status_effect_id,
                                          projectile.status_chance_percent, rng);
            }
        }
    }
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
    if (projectile.next_hop < projectile.path.size())
        return ActionResult(step_cost); // more hops queued for later turns

    const std::vector<Vec2> impact_tiles =
        projectile.pierces ? projectile.path : std::vector<Vec2>{projectile.path.back()};
    Registry& registry = actor.GetRegistry();
    ResolveImpact(registry, *m_grid, *m_affixes, *m_rng, projectile, impact_tiles);

    registry.DestroyEntity(actor.Handle());
    return ActionResult(step_cost);
}

} // namespace psr
