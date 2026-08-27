#include "Actions/PhotonArtAction.h"

#include "Combat/EffectiveStats.h"
#include "Combat/Hostility.h"
#include "Combat/TargetResolution.h"
#include "Components/EquipmentComponent.h"
#include "Components/SelectedTargetComponent.h"
#include "Engine/Combat/CombatMath.h"
#include "Engine/Combat/PhotonArt.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/PPComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/RaceComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/StatsComponent.h"
#include "Engine/ECS/WeaponComponent.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace psr {

namespace {
    bool WeaponGrants(const WeaponComponent& weapon, std::uint32_t photon_art_id)
    {
        return std::find(weapon.photon_art_ids.begin(), weapon.photon_art_ids.end(), photon_art_id) !=
               weapon.photon_art_ids.end();
    }

    float TierMultiplier(const std::vector<PhotonArtTier>& tiers)
    {
        return tiers.empty() ? 1.0f : tiers.front().power_multiplier;
    }
} // namespace

PhotonArtAction::PhotonArtAction(Grid& grid, const PhotonArtLibrary& photon_arts, const AffixLibrary& affixes,
                                 std::uint32_t photon_art_id, std::mt19937& rng)
    : m_grid(&grid), m_photon_arts(&photon_arts), m_affixes(&affixes), m_photon_art_id(photon_art_id), m_rng(&rng)
{
}

ActionResult PhotonArtAction::Perform(Entity actor)
{
    const PhotonArt* art = m_photon_arts->Find(m_photon_art_id);
    if (!art)
        return ActionResult(0);

    const EquipmentComponent* equipment = actor.TryGet<EquipmentComponent>();
    if (!equipment || equipment->weapon == entt::null)
        return ActionResult(0);

    Registry& registry = actor.GetRegistry();
    const WeaponComponent* weapon = registry.TryGetComponent<WeaponComponent>(equipment->weapon);
    if (!weapon || !WeaponGrants(*weapon, m_photon_art_id))
        return ActionResult(0);

    PPComponent* pp = actor.TryGet<PPComponent>();
    if (!pp || pp->current_pp < art->pp_cost)
        return ActionResult(0);
    pp->current_pp -= art->pp_cost;

    const Vec2 origin = actor.Get<Position>().tile;
    const Vec2 selected_tile =
        actor.Has<SelectedTargetComponent>() ? actor.Get<SelectedTargetComponent>().tile : origin;
    const Vec2 offset = selected_tile - origin;
    const float multiplier = TierMultiplier(art->tiers);

    StatsComponent attacker_stats = ComputeEffectiveStats(actor, *m_affixes);
    std::uniform_real_distribution<float> unit_roll(0.0f, 1.0f);
    std::uniform_real_distribution<float> variance_roll(0.9f, 1.1f);

    if (offset == Vec2{0, 0})
    {
        // SelfTarget: no attacker-vs-defender roll against yourself. Damage
        // hits the caster directly; Drain is a pure heal sized off the
        // caster's own ATP; Status carries status_effect_id unconsumed
        // (M7.3's job) with no immediate effect.
        if (art->effect_family == EffectFamily::Damage)
        {
            if (HealthComponent* health = actor.TryGet<HealthComponent>())
            {
                const int damage = static_cast<int>(std::lround(
                    ComputeDamage(attacker_stats.atp, attacker_stats.dfp, variance_roll(*m_rng)) * multiplier));
                health->current_hp = std::max(0, health->current_hp - damage);
                if (health->current_hp == 0)
                    registry.DestroyEntity(actor.Handle());
            }
        }
        else if (art->effect_family == EffectFamily::Drain)
        {
            const int notional =
                static_cast<int>(std::lround(ComputeDamage(attacker_stats.atp, 0, variance_roll(*m_rng)) * multiplier));
            const int heal = notional * art->drain_percent / 100;
            if (HealthComponent* health = actor.TryGet<HealthComponent>())
                health->current_hp = std::min(health->max_hp, health->current_hp + heal);
        }
        return ActionResult(kPhotonArtCost);
    }

    const Vec2 direction = SnapToCardinalDirection(offset);
    const std::vector<Vec2> target_tiles =
        ResolveTargetTiles(*m_grid, registry, origin, direction, art->range_shape, art->range);

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

            for (int hit = 0; hit < art->hits_per_turn; ++hit)
            {
                if (!target.IsValid())
                    break;

                const float hit_chance = ComputeHitChance(attacker_stats.ata, defender_stats.evp);
                if (unit_roll(*m_rng) > hit_chance)
                    continue; // miss

                int damage = static_cast<int>(std::lround(
                    ComputeDamage(attacker_stats.atp, defender_stats.dfp, variance_roll(*m_rng)) * multiplier));
                damage = ApplyRaceBonus(damage, weapon->race_bonuses, defender_race_id);

                HealthComponent& health = target.Get<HealthComponent>();
                health.current_hp = std::max(0, health.current_hp - damage);

                if (art->effect_family == EffectFamily::Drain)
                {
                    if (HealthComponent* attacker_health = actor.TryGet<HealthComponent>())
                        attacker_health->current_hp = std::min(
                            attacker_health->max_hp, attacker_health->current_hp + damage * art->drain_percent / 100);
                }

                if (health.current_hp == 0)
                {
                    m_grid->RemoveEntity(tile, occupant);
                    registry.DestroyEntity(occupant);
                    break;
                }
            }
        }
    }

    return ActionResult(kPhotonArtCost);
}

} // namespace psr
