#include "Actions/TechniqueAction.h"

#include "Combat/EffectiveStats.h"
#include "Combat/Hostility.h"
#include "Combat/TargetResolution.h"
#include "Components/EquipmentComponent.h"
#include "Components/SelectedTargetComponent.h"
#include "Engine/Combat/CombatMath.h"
#include "Engine/Combat/Technique.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/RaceComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/StatsComponent.h"
#include "Engine/ECS/TPComponent.h"
#include "Engine/ECS/WeaponComponent.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace psr {

namespace {
    bool WeaponGrants(const WeaponComponent& weapon, std::uint32_t technique_id)
    {
        return std::find(weapon.technique_ids.begin(), weapon.technique_ids.end(), technique_id) !=
               weapon.technique_ids.end();
    }

    float TierMultiplier(const std::vector<TechniqueTier>& tiers)
    {
        return tiers.empty() ? 1.0f : tiers.front().power_multiplier;
    }
} // namespace

TechniqueAction::TechniqueAction(Grid& grid, const TechniqueLibrary& techniques, const AffixLibrary& affixes,
                                 std::uint32_t technique_id, std::mt19937& rng)
    : m_grid(&grid), m_techniques(&techniques), m_affixes(&affixes), m_technique_id(technique_id), m_rng(&rng)
{
}

ActionResult TechniqueAction::Perform(Entity actor)
{
    const Technique* technique = m_techniques->Find(m_technique_id);
    if (!technique)
        return ActionResult(0);

    const EquipmentComponent* equipment = actor.TryGet<EquipmentComponent>();
    if (!equipment || equipment->weapon == entt::null)
        return ActionResult(0);

    Registry& registry = actor.GetRegistry();
    const WeaponComponent* weapon = registry.TryGetComponent<WeaponComponent>(equipment->weapon);
    if (!weapon || !WeaponGrants(*weapon, m_technique_id))
        return ActionResult(0);

    TPComponent* tp = actor.TryGet<TPComponent>();
    if (!tp || tp->current_tp < technique->tp_cost)
        return ActionResult(0);
    tp->current_tp -= technique->tp_cost;

    const Vec2 origin = actor.Get<Position>().tile;
    const Vec2 selected_tile =
        actor.Has<SelectedTargetComponent>() ? actor.Get<SelectedTargetComponent>().tile : origin;
    const Vec2 offset = selected_tile - origin;
    const float multiplier = TierMultiplier(technique->tiers);

    StatsComponent attacker_stats = ComputeEffectiveStats(actor, *m_affixes);
    std::uniform_real_distribution<float> unit_roll(0.0f, 1.0f);
    std::uniform_real_distribution<float> variance_roll(0.9f, 1.1f);

    if (offset == Vec2{0, 0})
    {
        // SelfTarget: no attacker-vs-defender roll against yourself. Only
        // Damage has an immediate effect here -- Drain has no drain_percent
        // to size a heal by on Technique, and Status carries
        // status_effect_id unconsumed (M7.3's job).
        if (technique->effect_family == EffectFamily::Damage)
        {
            if (HealthComponent* health = actor.TryGet<HealthComponent>())
            {
                const int damage = static_cast<int>(
                    std::lround(ComputeTechniqueDamage(attacker_stats.mst, attacker_stats.dfp, variance_roll(*m_rng)) *
                                multiplier));
                health->current_hp = std::max(0, health->current_hp - damage);
                if (health->current_hp == 0)
                    registry.DestroyEntity(actor.Handle());
            }
        }
        return ActionResult(kTechniqueCost);
    }

    const Vec2 direction = SnapToCardinalDirection(offset);
    const std::vector<Vec2> target_tiles =
        ResolveTargetTiles(*m_grid, registry, origin, direction, technique->range_shape, technique->range);

    for (Vec2 tile : target_tiles)
    {
        // Snapshot before hitting anything -- a lethal hit mutates the
        // Grid's own occupant vector via RemoveEntity (see AttackAction's
        // identical precaution).
        const std::vector<entt::entity> occupants = m_grid->GetEntities(tile);
        for (entt::entity occupant : occupants)
        {
            if (occupant == actor.Handle() || !registry.HasComponent<HealthComponent>(occupant))
                continue;
            Entity target(registry, occupant);
            if (!IsHostile(actor, target))
                continue;

            const StatsComponent defender_stats = ComputeEffectiveStats(target, *m_affixes);
            const RaceComponent* defender_race = target.TryGet<RaceComponent>();
            const std::uint32_t defender_race_id = defender_race ? defender_race->race_id : 0;

            // hits_per_turn has no Technique-side equivalent (a single
            // resolution per target, unlike PhotonArt's weapon-style
            // multi-hit) -- Techniques are single-cast spells per GDD.
            const float hit_chance = ComputeHitChance(attacker_stats.ata, defender_stats.evp);
            if (unit_roll(*m_rng) > hit_chance)
                continue; // miss

            int damage = static_cast<int>(std::lround(
                ComputeTechniqueDamage(attacker_stats.mst, defender_stats.dfp, variance_roll(*m_rng)) * multiplier));
            damage = ApplyRaceBonus(damage, weapon->race_bonuses, defender_race_id);

            HealthComponent& health = target.Get<HealthComponent>();
            health.current_hp = std::max(0, health.current_hp - damage);
            if (health.current_hp == 0)
            {
                m_grid->RemoveEntity(tile, occupant);
                registry.DestroyEntity(occupant);
            }
        }
    }

    return ActionResult(kTechniqueCost);
}

} // namespace psr
