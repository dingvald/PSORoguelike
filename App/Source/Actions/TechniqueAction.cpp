#include "Actions/TechniqueAction.h"

#include "Combat/CombatMath.h"
#include "Combat/EffectiveStats.h"
#include "Combat/Hostility.h"
#include "Combat/StatusEffectApplication.h"
#include "Combat/StatusEffectHooks.h"
#include "Combat/TargetResolution.h"
#include "Combat/Technique.h"
#include "Combat/TechniqueCastEvent.h"
#include "Components/ElementalResistanceComponent.h"
#include "Components/KnownTechniquesComponent.h"
#include "Components/SelectedTargetComponent.h"
#include "Components/StatsComponent.h"
#include "Components/TPComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/Combat/HealEvent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace psr {

namespace {
    // Finds the actor's known tier for technique_id, if any -- nullptr if the
    // technique was never learned (see Items/TechniqueLearning.h's
    // LearnTechnique, the only writer of KnownTechniquesComponent).
    const KnownTechniqueEntry* FindKnownEntry(Entity actor, std::uint32_t technique_id)
    {
        const KnownTechniquesComponent* known = actor.TryGet<KnownTechniquesComponent>();
        if (!known)
            return nullptr;
        auto it = std::find_if(known->known.begin(), known->known.end(),
                               [technique_id](const KnownTechniqueEntry& entry)
                               { return entry.technique_id == technique_id; });
        return it == known->known.end() ? nullptr : &*it;
    }

    // The tier entry matching known_tier exactly, falling back to the
    // highest authored tier <= known_tier, falling back to a flat 1.0x if
    // none qualifies (including an empty tiers list) -- so a technique disk's
    // authored level actually changes cast power once tiers exist, rather
    // than always resolving tiers[0] regardless of what was learned.
    float TierMultiplierForLevel(const std::vector<TechniqueTier>& tiers, int known_tier)
    {
        const TechniqueTier* best = nullptr;
        for (const TechniqueTier& tier : tiers)
        {
            if (tier.tier > known_tier)
                continue;
            if (!best || tier.tier > best->tier)
                best = &tier;
        }
        return best ? best->power_multiplier : 1.0f;
    }
} // namespace

TechniqueAction::TechniqueAction(Grid& grid, const TechniqueLibrary& techniques, const AffixLibrary& affixes,
                                 std::uint32_t technique_id, std::mt19937& rng)
    : m_grid(&grid), m_techniques(&techniques), m_affixes(&affixes), m_technique_id(technique_id), m_rng(&rng)
{
}

ActionResult TechniqueAction::Perform(Entity actor)
{
    // EquipmentComponent's own handler fills attacker_stats; TPComponent's
    // own handler fills current_tp/has_tp_component -- this action never
    // reads EquipmentComponent/TPComponent directly, and only touches
    // TPComponent for the deduction below, once the gate has already passed.
    BeforeTechniqueCastEvent before_cast{m_technique_id};
    actor.Dispatch(before_cast);
    if (before_cast.cancelled) // Shocked -- attack-type actions no-op for zero cost, movement still works
        return ActionResult(0);

    // Casting is gated purely on learned knowledge (see
    // KnownTechniquesComponent.h/Items/TechniqueLearning.h) -- not on the
    // equipped weapon, per the user's explicit "replace weapon-granting"
    // choice. An unknown technique_id is a free no-op, same shape the old
    // weapon-grant check had.
    const KnownTechniqueEntry* known_entry = FindKnownEntry(actor, m_technique_id);
    if (!known_entry)
        return ActionResult(0);

    const Technique* technique = m_techniques->Find(m_technique_id);
    if (!technique)
        return ActionResult(0);

    if (!before_cast.has_tp_component || before_cast.current_tp < technique->tp_cost)
        return ActionResult(0);

    TPComponent* tp = actor.TryGet<TPComponent>();
    if (!tp)
        return ActionResult(0);
    tp->current_tp -= technique->tp_cost;

    AfterTechniqueCastEvent after_cast{m_technique_id};
    actor.Dispatch(after_cast);

    Registry& registry = actor.GetRegistry();
    const Vec2 origin = actor.Get<Position>().tile;
    const Vec2 selected_tile =
        actor.Has<SelectedTargetComponent>() ? actor.Get<SelectedTargetComponent>().tile : origin;
    const Vec2 offset = selected_tile - origin;
    const float multiplier = TierMultiplierForLevel(technique->tiers, known_entry->tier);

    const StatsComponent& attacker_stats = before_cast.attacker_stats;
    std::uniform_real_distribution<float> unit_roll(0.0f, 1.0f);

    if (offset == Vec2{0, 0})
    {
        // SelfTarget: no attacker-vs-defender roll against yourself. Damage
        // and Heal are the only effect families with an immediate effect
        // here -- Drain has no drain_percent to size a heal by on Technique,
        // and self-target status application stays out of scope this pass
        // (no buff-shaped use case exists yet to justify it) -- see the
        // directional branch below for the target-facing Status
        // implementation.
        if (technique->effect_family == EffectFamily::Damage)
        {
            if (actor.Has<HealthComponent>())
            {
                const ElementalResistanceComponent* self_resistance = actor.TryGet<ElementalResistanceComponent>();
                const int self_resistance_percent =
                    self_resistance ? self_resistance->ResistanceFor(technique->element) : 0;
                int damage = static_cast<int>(
                    std::lround(ComputeTechniqueDamage(attacker_stats.mst, self_resistance_percent) * multiplier));

                BeforeDamageEvent before{actor, damage};
                actor.Dispatch(before);
                damage = before.incoming_damage;

                IncomingDamageEvent incoming{actor, damage};
                actor.Dispatch(incoming);
            }
        }
        else if (technique->effect_family == EffectFamily::Heal)
        {
            // No target's-own-resistance concept for a heal (there is no
            // ElementalResistanceComponent-analogous mitigation on the
            // caster's own restore) -- reuses ComputeTechniqueDamage's mst/5
            // shape with resistance_percent=0, same magnitude formula,
            // different event.
            const int amount =
                static_cast<int>(std::lround(ComputeTechniqueDamage(attacker_stats.mst, 0) * multiplier));
            IncomingHealEvent heal{actor, amount};
            actor.Dispatch(heal);
        }
        return ActionResult(kTechniqueCost);
    }

    // A directional Heal cast has nothing to resolve against -- there is no
    // ally-targeting concept in this codebase (Hostility.h's IsHostile is
    // player-vs-everyone), so Heal only ever applies on the self-target
    // branch above. The turn is still spent, matching every other directional
    // cast's "cost is charged once a cast executes" convention.
    if (technique->effect_family == EffectFamily::Heal)
        return ActionResult(kTechniqueCost);

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

            // hits_per_turn has no Technique-side equivalent (a single
            // resolution per target, unlike PhotonArt's weapon-style
            // multi-hit) -- Techniques are single-cast spells per GDD.
            const float hit_chance = ComputeHitChance(attacker_stats.ata, defender_stats.evp);
            if (unit_roll(*m_rng) > hit_chance)
            {
                AttackMissEvent miss{target};
                actor.Dispatch(miss);
                continue; // miss
            }

            if (technique->effect_family == EffectFamily::Status)
            {
                // Landing the cast is the check -- a pure-status Technique
                // deals no damage, it guarantees its ailment on hit.
                ApplyStatusEffect(target, registry.GetStatusEffectLibrary(), technique->status_effect_id);
                continue;
            }

            const ElementalResistanceComponent* defender_resistance = target.TryGet<ElementalResistanceComponent>();
            const int resistance_percent =
                defender_resistance ? defender_resistance->ResistanceFor(technique->element) : 0;
            int damage = static_cast<int>(
                std::lround(ComputeTechniqueDamage(attacker_stats.mst, resistance_percent) * multiplier));

            BeforeDamageEvent before{target, damage};
            actor.Dispatch(before);
            damage = before.incoming_damage;

            IncomingDamageEvent incoming{actor, damage};
            target.Dispatch(incoming);

            if (!target.IsValid())
                continue;

            // element/status_effect_id/status_chance_percent are spell-
            // authored on Technique itself (independent of the wielding
            // weapon, unlike PhotonArt) -- see Technique.h's own doc
            // comment.
            MaybeApplyElementalStatus(target, registry.GetStatusEffectLibrary(), technique->status_effect_id,
                                      technique->status_chance_percent, *m_rng);
        }
    }

    return ActionResult(kTechniqueCost);
}

} // namespace psr
