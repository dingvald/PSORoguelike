#include "Combat/ProjectileImpact.h"

#include "Combat/CombatMath.h"
#include "Combat/EffectFamily.h"
#include "Combat/EffectiveStats.h"
#include "Combat/Hostility.h"
#include "Combat/StatusEffectApplication.h"
#include "Combat/StatusEffectHooks.h"
#include "Components/ElementalResistanceComponent.h"
#include "Components/ProjectileComponent.h"
#include "Components/RaceComponent.h"
#include "Components/StatsComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/HealthComponent.h"

#include <cmath>
#include <vector>

namespace psr {

bool ResolveProjectileImpact(Registry& registry, const Grid& grid, const AffixLibrary& affixes, std::mt19937& rng,
                             const ProjectileComponent& projectile, Vec2 tile)
{
    if (!registry.IsValid(projectile.source))
        return false; // caster died mid-flight -- nothing to attribute the hit to

    Entity source(registry, projectile.source);
    std::uniform_real_distribution<float> unit_roll(0.0f, 1.0f);

    bool impacted = false;
    // Snapshot before hitting anything -- a lethal hit mutates the Grid's
    // own occupant vector via RemoveEntity, same precaution every other
    // Action's hit loop takes.
    const std::vector<entt::entity> occupants = grid.GetEntities(tile);
    for (entt::entity occupant : occupants)
    {
        if (occupant == projectile.source || !registry.HasComponent<HealthComponent>(occupant))
            continue;
        Entity target(registry, occupant);
        if (!IsHostile(source, target))
            continue;
        impacted = true;

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

        int damage;
        bool is_critical = false;
        if (projectile.physical_damage)
        {
            // Physical (ATP-vs-DFP, race-bonus, crit) formula -- same as
            // WeaponAttackAction's melee branch, unlike the MST-based magic
            // formula below every Technique projectile still uses. The
            // weapon's own elemental flavor (if any) still gets its usual
            // resisted proc-and-bonus-damage roll, same as melee -- see
            // RollElementalDamageBonus below.
            const RaceComponent* defender_race = target.TryGet<RaceComponent>();
            const std::uint32_t defender_race_id = defender_race ? defender_race->race_id : 0;
            const int boosted_atp =
                ApplyRaceBonus(projectile.attacker_stats.atp, projectile.race_bonuses, defender_race_id);
            std::uniform_real_distribution<float> variance_roll(0.9f, 1.1f);
            damage = ComputeDamage(boosted_atp, defender_stats.dfp, variance_roll(rng));
            is_critical = unit_roll(rng) < ComputeCritChance(projectile.attacker_stats.lck);
            damage = ApplyCritical(damage, is_critical);
            damage += RollElementalDamageBonus(target, registry.GetStatusEffectLibrary(), projectile.element,
                                               projectile.status_effect_id, projectile.status_chance_percent,
                                               resistance_percent, boosted_atp, projectile.is_special_attack, rng);
        }
        else
        {
            damage = static_cast<int>(std::lround(
                ComputeTechniqueDamage(projectile.attacker_stats.mst, resistance_percent) * projectile.power_multiplier));

            // The spell's own ailment still gets its usual resisted roll --
            // unlike the physical branch above, the bonus this returns is
            // discarded: a Technique's damage is already entirely elemental
            // (ComputeTechniqueDamage), so folding in a second elemental
            // kicker here would double-count it.
            RollElementalDamageBonus(target, registry.GetStatusEffectLibrary(), projectile.element,
                                     projectile.status_effect_id, projectile.status_chance_percent, resistance_percent,
                                     projectile.attacker_stats.mst, /*is_special_attack=*/false, rng);
        }

        BeforeDamageEvent before{target, damage};
        source.Dispatch(before);
        damage = before.incoming_damage;

        IncomingDamageEvent incoming{source,      damage, is_critical, projectile.hit_effect_prefab_id,
                                     projectile.hit_effect_duration, projectile.hit_stun_energy};
        target.Dispatch(incoming);
    }
    return impacted;
}

} // namespace psr
