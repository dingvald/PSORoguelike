#include "Actions/AttackAction.h"

#include "Combat/AttackEvent.h"
#include "Combat/CombatMath.h"
#include "Combat/EffectiveStats.h"
#include "Combat/Hostility.h"
#include "Combat/StatusEffectHooks.h"
#include "Combat/TargetResolution.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RaceComponent.h"
#include "Components/StatsComponent.h"
#include "Components/TweenComponent.h"
#include "Components/WeaponComponent.h" // RaceBonusEntry
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Math/Vec2f.h"

#include <cmath>
#include <memory>
#include <random>
#include <utility>
#include <vector>

namespace psr {

AttackAction::AttackAction(Grid& grid, const AffixLibrary& affixes, Vec2 direction, std::mt19937& rng,
                           int attack_number)
    : m_grid(&grid), m_affixes(&affixes), m_direction(direction), m_rng(&rng), m_attack_number(attack_number)
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
    const std::vector<Vec2> target_tiles =
        ResolveTargetTiles(*m_grid, registry, origin, m_direction, before_attack.range_shape, before_attack.range);

    std::vector<entt::entity> targets;
    for (Vec2 tile : target_tiles)
    {
        for (entt::entity occupant : m_grid->GetEntities(tile))
        {
            if (occupant == actor.Handle() || !registry.HasComponent<HealthComponent>(occupant))
                continue;
            if (!IsHostile(actor, Entity(registry, occupant)))
                continue;
            targets.push_back(occupant);
        }
    }

    if (targets.empty())
        return ActionResult(0);

    // Captured by the on_completion callback below rather than resolved now:
    // hit rolls/damage/status only run once the lunge Tween actually reaches
    // the target (see this class's own doc comment). registry/m_affixes/m_rng
    // are long-lived (GameplayLayer-owned), same lifetime assumption
    // MoveAction/AttackAction's own constructor pointers already rely on;
    // actor_handle/targets/the weapon-derived combat parameters are captured
    // by value since they describe this swing as committed at declare time.
    Registry* registry_ptr = &registry;
    const AffixLibrary* affixes = m_affixes;
    std::mt19937* rng = m_rng;
    const entt::entity actor_handle = actor.Handle();
    const int hits_per_turn = before_attack.hits_per_turn;
    std::vector<RaceBonusEntry> race_bonuses = before_attack.race_bonuses;
    const std::uint32_t status_effect_id = before_attack.status_effect_id;
    const int status_chance_percent = before_attack.status_chance_percent;
    const StatsComponent attacker_stats = before_attack.attacker_stats;
    const std::uint32_t hit_effect_prefab_id = before_attack.hit_effect_prefab_id;
    const float hit_effect_duration = before_attack.hit_effect_duration;

    auto apply_damage = [registry_ptr, affixes, rng, actor_handle, targets, hits_per_turn, race_bonuses,
                         status_effect_id, status_chance_percent, attacker_stats, hit_effect_prefab_id,
                         hit_effect_duration]()
    {
        Registry& registry = *registry_ptr;
        Entity actor(registry, actor_handle);
        std::uniform_real_distribution<float> unit_roll(0.0f, 1.0f);
        std::uniform_real_distribution<float> variance_roll(0.9f, 1.1f);

        for (entt::entity target_handle : targets)
        {
            Entity target(registry, target_handle);
            if (!target.IsValid())
                continue;

            const StatsComponent defender_stats = ComputeEffectiveStats(target, *affixes);
            const RaceComponent* defender_race = target.TryGet<RaceComponent>();
            const std::uint32_t defender_race_id = defender_race ? defender_race->race_id : 0;

            for (int hit = 0; hit < hits_per_turn; ++hit)
            {
                if (!target.IsValid())
                    break;

                const int combo_ata =
                    static_cast<int>(std::lround(static_cast<float>(attacker_stats.ata) * ComboAtaMultiplier(hit)));
                const float hit_chance = ComputeHitChance(combo_ata, defender_stats.evp);
                if (unit_roll(*rng) > hit_chance)
                {
                    AttackMissEvent miss{target};
                    actor.Dispatch(miss);
                    continue; // miss
                }

                const int boosted_atp = ApplyRaceBonus(attacker_stats.atp, race_bonuses, defender_race_id);
                int damage = ComputeDamage(boosted_atp, defender_stats.dfp, variance_roll(*rng));
                const bool is_critical = unit_roll(*rng) < ComputeCritChance(attacker_stats.lck);
                damage = ApplyCritical(damage, is_critical);

                BeforeDamageEvent before{target, damage};
                actor.Dispatch(before);
                damage = before.incoming_damage;

                IncomingDamageEvent incoming{actor, damage, is_critical, hit_effect_prefab_id, hit_effect_duration};
                target.Dispatch(incoming);

                if (!target.IsValid())
                    break;

                // The weapon's own elemental flavor (if any) gets a chance
                // to inflict its ailment on a landed, non-lethal hit.
                MaybeApplyElementalStatus(target, registry.GetStatusEffectLibrary(), status_effect_id,
                                          status_chance_percent, *rng);
            }
        }
    };

    const Vec2f peak_offset =
        Vec2f{static_cast<float>(m_direction.x), static_cast<float>(m_direction.y)} * kLungeDistance;
    TweenComponent& tween_component = actor.GetOrEmplace<TweenComponent>();
    tween_component.queue.push_back(Tween{Vec2f{}, peak_offset, kLungeOutDuration, 0.0f, std::move(apply_damage)});
    tween_component.queue.push_back(Tween{peak_offset, Vec2f{}, kLungeBackDuration, 0.0f, nullptr});

    AfterAttackEvent after_attack{true};
    actor.Dispatch(after_attack);

    std::unique_ptr<IAction> extra_attack;
    if (actor.Has<PlayerControlledComponent>() && m_attack_number < kMaxAttacksPerTurn)
    {
        std::uniform_real_distribution<float> extra_attack_roll(0.0f, 1.0f);
        if (extra_attack_roll(*m_rng) < (kExtraAttackChance - kExtraAttackChanceReduction * (m_attack_number - 1)))
            extra_attack =
                std::make_unique<AttackAction>(*m_grid, *m_affixes, m_direction, *m_rng, m_attack_number + 1);
    }

    return ActionResult(kAttackCost, std::move(extra_attack));
}

} // namespace psr