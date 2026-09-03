#include "Actions/PhotonArtAction.h"

#include "Combat/CombatMath.h"
#include "Combat/EffectiveStats.h"
#include "Combat/Hostility.h"
#include "Combat/PhotonArt.h"
#include "Combat/PhotonArtCastEvent.h"
#include "Combat/StatusEffectApplication.h"
#include "Combat/StatusEffectHooks.h"
#include "Combat/TargetResolution.h"
#include "Components/RaceComponent.h"
#include "Components/SelectedTargetComponent.h"
#include "Components/StatsComponent.h"
#include "Components/TPComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace psr {

namespace {
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
    // EquipmentComponent's own handler resolves the equipped weapon (if any)
    // and fills has_weapon/weapon_grants_id/race_bonuses/attacker_stats;
    // TPComponent's own handler fills current_tp/has_tp_component -- this
    // action never reads EquipmentComponent/WeaponComponent directly, and
    // only touches TPComponent for the deduction below, once the gate has
    // already passed.
    BeforePhotonArtCastEvent before_cast{m_photon_art_id};
    actor.Dispatch(before_cast);
    if (before_cast.cancelled) // Shocked -- attack-type actions no-op for zero cost, movement still works
        return ActionResult(0);
    if (!before_cast.has_weapon || !before_cast.weapon_grants_id)
        return ActionResult(0);

    const PhotonArt* art = m_photon_arts->Find(m_photon_art_id);
    if (!art)
        return ActionResult(0);

    if (!before_cast.has_tp_component || before_cast.current_tp < art->tp_cost)
        return ActionResult(0);

    TPComponent* tp = actor.TryGet<TPComponent>();
    if (!tp)
        return ActionResult(0);
    tp->current_tp -= art->tp_cost;

    AfterPhotonArtCastEvent after_cast{m_photon_art_id};
    actor.Dispatch(after_cast);

    Registry& registry = actor.GetRegistry();
    const Vec2 origin = actor.Get<Position>().tile;
    const Vec2 selected_tile =
        actor.Has<SelectedTargetComponent>() ? actor.Get<SelectedTargetComponent>().tile : origin;
    const Vec2 offset = selected_tile - origin;
    const float multiplier = TierMultiplier(art->tiers);

    const StatsComponent& attacker_stats = before_cast.attacker_stats;
    std::uniform_real_distribution<float> unit_roll(0.0f, 1.0f);
    std::uniform_real_distribution<float> variance_roll(0.9f, 1.1f);

    if (offset == Vec2{0, 0})
    {
        // SelfTarget: no attacker-vs-defender roll against yourself. Damage
        // hits the caster directly; Drain is a pure heal sized off the
        // caster's own ATP; Status has no immediate effect here -- unlike the
        // directional branch below, self-target status application stays out
        // of scope this pass (no buff-shaped use case exists yet to justify
        // it).
        if (art->effect_family == EffectFamily::Damage)
        {
            if (actor.Has<HealthComponent>())
            {
                int damage = static_cast<int>(std::lround(
                    ComputeDamage(attacker_stats.atp, attacker_stats.dfp, variance_roll(*m_rng)) * multiplier));

                BeforeDamageEvent before{actor, damage};
                actor.Dispatch(before);
                damage = before.incoming_damage;

                IncomingDamageEvent incoming{actor, damage};
                actor.Dispatch(incoming);
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

                const int combo_ata =
                    static_cast<int>(std::lround(static_cast<float>(attacker_stats.ata) * ComboAtaMultiplier(hit)));
                const float hit_chance = ComputeHitChance(combo_ata, defender_stats.evp);
                if (unit_roll(*m_rng) > hit_chance)
                {
                    AttackMissEvent miss{target};
                    actor.Dispatch(miss);
                    continue; // miss
                }

                if (art->effect_family == EffectFamily::Status)
                {
                    // Landing the cast is the check -- a pure-status art
                    // deals no damage, it guarantees its ailment on hit.
                    ApplyStatusEffect(target, registry.GetStatusEffectLibrary(), art->status_effect_id);
                    continue;
                }

                const int boosted_atp = ApplyRaceBonus(attacker_stats.atp, before_cast.race_bonuses, defender_race_id);
                int damage = static_cast<int>(
                    std::lround(ComputeDamage(boosted_atp, defender_stats.dfp, variance_roll(*m_rng)) * multiplier));
                damage = ApplyCritical(damage, unit_roll(*m_rng) < ComputeCritChance(attacker_stats.lck));

                BeforeDamageEvent before{target, damage};
                actor.Dispatch(before);
                damage = before.incoming_damage;

                IncomingDamageEvent incoming{actor, damage};
                target.Dispatch(incoming);

                if (art->effect_family == EffectFamily::Drain)
                {
                    if (HealthComponent* attacker_health = actor.TryGet<HealthComponent>())
                        attacker_health->current_hp = std::min(
                            attacker_health->max_hp, attacker_health->current_hp + damage * art->drain_percent / 100);
                }

                if (!target.IsValid())
                    break;

                // The wielded weapon's own elemental flavor (if any) gets a
                // chance to inflict its ailment on a landed, non-lethal hit.
                MaybeApplyElementalStatus(target, registry.GetStatusEffectLibrary(), before_cast.status_effect_id,
                                          before_cast.status_chance_percent, *m_rng);
            }
        }
    }

    return ActionResult(kPhotonArtCost);
}

} // namespace psr
