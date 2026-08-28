#include "Actions/AttackAction.h"

#include "Combat/EffectiveStats.h"
#include "Combat/Hostility.h"
#include "Combat/StatusEffectHooks.h"
#include "Combat/TargetResolution.h"
#include "Engine/Combat/AttackEvent.h"
#include "Engine/Combat/CombatMath.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/RaceComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/StatsComponent.h"

#include <algorithm>
#include <vector>

namespace psr {

AttackAction::AttackAction(Grid& grid, const AffixLibrary& affixes, Vec2 direction, std::mt19937& rng)
    : m_grid(&grid), m_affixes(&affixes), m_direction(direction), m_rng(&rng)
{
}

ActionResult AttackAction::Perform(Entity actor)
{
    // EquipmentComponent's own handler resolves the equipped weapon (if any)
    // and fills range_shape/range/hits_per_turn/race_bonuses/attacker_stats --
    // this action never reads EquipmentComponent/WeaponComponent directly.
    BeforeAttackEvent before_attack{m_direction};
    actor.Dispatch(before_attack);
    if (before_attack.cancelled) // Shocked -- attack-type actions no-op for zero cost, movement still works
        return ActionResult(0);
    if (!before_attack.has_weapon)
        return ActionResult(0);

    Registry& registry = actor.GetRegistry();
    const Vec2 origin = actor.Get<Position>().tile;
    const std::vector<Vec2> target_tiles = ResolveTargetTiles(*m_grid, registry, origin, m_direction,
                                                               before_attack.range_shape, before_attack.range);

    const StatsComponent& attacker_stats = before_attack.attacker_stats;
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

            for (int hit = 0; hit < before_attack.hits_per_turn; ++hit)
            {
                if (!target.IsValid())
                    break;

                const float hit_chance = ComputeHitChance(attacker_stats.ata, defender_stats.evp);
                if (unit_roll(*m_rng) > hit_chance)
                    continue; // miss

                int damage = ComputeDamage(attacker_stats.atp, defender_stats.dfp, variance_roll(*m_rng));
                damage = ApplyRaceBonus(damage, before_attack.race_bonuses, defender_race_id);

                BeforeDamageEvent before{target, damage};
                actor.Dispatch(before);
                damage = before.incoming_damage;

                HealthComponent& health = target.Get<HealthComponent>();
                health.current_hp = std::max(0, health.current_hp - damage);
                const bool defeated = health.current_hp == 0;

                AfterDamageEvent after{target, damage, defeated};
                actor.Dispatch(after);

                if (defeated)
                {
                    m_grid->RemoveEntity(tile, occupant);
                    registry.DestroyEntity(occupant);
                    break;
                }

                // The weapon's own elemental flavor (if any) gets a chance
                // to inflict its ailment on a landed, non-lethal hit.
                MaybeApplyElementalStatus(target, registry.GetStatusEffectLibrary(), before_attack.status_effect_id,
                                          before_attack.status_chance_percent, *m_rng);
            }
        }
    }

    AfterAttackEvent after_attack{found_target};
    actor.Dispatch(after_attack);

    return found_target ? ActionResult(kAttackCost) : ActionResult(0);
}

} // namespace psr
