#include "Actions/WeaponAttackAction.h"

#include "Combat/ActionCost.h"
#include "Combat/AttackEvent.h"
#include "Combat/CombatMath.h"
#include "Combat/EffectFamily.h"
#include "Combat/EffectiveStats.h"
#include "Combat/Hostility.h"
#include "Combat/StatusEffectHooks.h"
#include "Combat/TargetResolution.h"
#include "Components/ActorComponent.h"
#include "Components/BlocksMovementComponent.h"
#include "Components/KnockbackMultiplierComponent.h"
#include "Components/ProjectileComponent.h"
#include "Components/RaceComponent.h"
#include "Components/SelectedTargetComponent.h"
#include "Components/StatsComponent.h"
#include "Components/TweenComponent.h"
#include "Components/WeaponComponent.h" // RaceBonusEntry
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Math/Vec2f.h"
#include "Engine/Turns/TurnQueue.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <utility>
#include <vector>

namespace psr {

namespace {
    // Melee knockback's tween duration -- mirrors MoveAction::kMoveTweenDuration
    // (a plain glide, not the lunge-and-return pair the swing itself uses)
    // without depending on MoveAction just for one constant.
    constexpr float kKnockbackTweenDuration = 0.12f;

    // Pushes target 1 tile further away from origin (the attacker's tile at
    // hit time) -- element-wise sign of the offset from origin to the
    // target's own tile, so it degrades correctly for any WeaponRangeShape
    // (a diagonal Cone3 hit, a far Line hit) as a single-tile unit step.
    // Silently does nothing if the destination is out of bounds or blocked, or
    // if the target's own KnockbackMultiplierComponent::multiplier is <= 0
    // (player.json/box_*.json -- see KnockbackMultiplierComponent.h) --
    // damage and hit-stun (applied separately, see IncomingDamageEvent's own
    // hit_stun_energy field) still land either way. Melee-only: a projectile
    // hit never calls this (see ProjectileImpact.cpp).
    void ApplyKnockback(Registry& registry, Grid& grid, entt::entity target_handle, Vec2 origin)
    {
        if (!registry.IsValid(target_handle))
            return;
        Position* position = registry.TryGetComponent<Position>(target_handle);
        if (!position)
            return;

        const KnockbackMultiplierComponent* knockback_multiplier =
            registry.TryGetComponent<KnockbackMultiplierComponent>(target_handle);
        if (knockback_multiplier && knockback_multiplier->multiplier <= 0.0f)
            return;

        const Vec2 target_tile = position->tile;
        const Vec2 offset = target_tile - origin;
        const Vec2 push_direction{(offset.x > 0) - (offset.x < 0), (offset.y > 0) - (offset.y < 0)};
        if (push_direction == Vec2{0, 0})
            return;

        const Vec2 destination = target_tile + push_direction;
        if (!grid.Contains(destination))
            return;
        for (entt::entity occupant : grid.GetEntities(destination))
            if (registry.HasComponent<BlocksMovementComponent>(occupant))
                return;

        grid.RemoveEntity(target_tile, target_handle);
        grid.AddEntity(destination, target_handle);
        position->tile = destination;

        Entity target(registry, target_handle);
        target.GetOrEmplace<TweenComponent>().queue.push_back(
            Tween{Vec2f{static_cast<float>(target_tile.x - destination.x), static_cast<float>(target_tile.y - destination.y)},
                  Vec2f{}, kKnockbackTweenDuration, 0.0f, nullptr});
    }
} // namespace

WeaponAttackAction::WeaponAttackAction(Grid& grid, const AffixLibrary& affixes, std::mt19937& rng,
                                       std::optional<Vec2> direction)
    : m_grid(&grid), m_affixes(&affixes), m_rng(&rng), m_direction(direction)
{
}

ActionResult WeaponAttackAction::Perform(Entity actor)
{
    Registry& registry = actor.GetRegistry();
    const Vec2 origin = actor.Get<Position>().tile;

    Vec2 direction;
    if (m_direction.has_value())
    {
        direction = *m_direction;
    }
    else
    {
        const Vec2 selected_tile =
            actor.Has<SelectedTargetComponent>() ? actor.Get<SelectedTargetComponent>().tile : origin;
        direction = SnapToDirection(selected_tile - origin);
    }

    // EquipmentComponent's own handler resolves the equipped weapon (if any)
    // and fills range_shape/range/hits_per_turn/race_bonuses/attacker_stats/
    // fires_projectile/... -- this action never reads
    // EquipmentComponent/WeaponComponent directly.
    BeforeAttackEvent before_attack{direction};
    actor.Dispatch(before_attack);
    if (before_attack.cancelled) // Shocked -- attack-type actions no-op for zero cost, movement still works
        return ActionResult(0);
    if (!before_attack.has_weapon)
        return ActionResult(0);

    // A fixed direction means this call came from MoveAction's bump
    // fallback -- a ranged weapon must be fired explicitly through the
    // hotbar's tile-select targeting (m_direction == nullopt) instead.
    if (before_attack.fires_projectile && m_direction.has_value())
        return ActionResult(0);

    if (before_attack.fires_projectile)
    {
        bool spawned = false;
        if (registry.HasPrefab(before_attack.projectile_prefab_id))
        {
            const std::vector<Vec2> path = BuildProjectilePath(*m_grid, registry, origin, direction,
                                                                before_attack.range, before_attack.projectile_pierces);
            if (!path.empty())
            {
                // Spawned at origin, not path.front(): ProjectileAdvanceAction's
                // first hop moves it to path[0], so it visually launches from
                // the attacker rather than appearing one tile out already --
                // same idiom as TechniqueAction's own projectile branch.
                const entt::entity projectile = registry.CreateEntity(before_attack.projectile_prefab_id);
                registry.Emplace<Position>(projectile, Position{origin});
                m_grid->AddEntity(origin, projectile);

                ProjectileComponent component;
                component.path = path;
                component.step_cost = std::max(1, TurnQueue::kDefaultActionThreshold / before_attack.projectile_speed);
                component.source = actor.Handle();
                component.pierces = before_attack.projectile_pierces;
                component.attacker_stats = before_attack.attacker_stats;
                component.power_multiplier = 1.0f;
                component.effect_family = EffectFamily::Damage;
                component.element = before_attack.element;
                component.status_effect_id = before_attack.status_effect_id;
                component.status_chance_percent = before_attack.status_chance_percent;
                component.hit_effect_prefab_id = before_attack.hit_effect_prefab_id;
                component.hit_effect_duration = before_attack.hit_effect_duration;
                component.physical_damage = true;
                component.race_bonuses = before_attack.race_bonuses;
                component.hit_stun_energy = before_attack.hit_stun_energy;
                registry.Emplace<ProjectileComponent>(projectile, std::move(component));

                registry.Emplace<ActorComponent>(projectile);
                spawned = true;
            }
        }

        AfterAttackEvent after_attack{spawned};
        actor.Dispatch(after_attack);
        return ActionResult(EffectiveActCost(actor, kWeaponAttackCost));
    }

    const std::vector<Vec2> target_tiles =
        ResolveTargetTiles(*m_grid, registry, origin, direction, before_attack.range_shape, before_attack.range);

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
    // the target. registry/m_grid/m_affixes/m_rng are long-lived
    // (GameplayLayer-owned); actor_handle/origin/targets/the weapon-derived
    // combat parameters are captured by value since they describe this swing
    // as committed at declare time.
    Registry* registry_ptr = &registry;
    Grid* grid_ptr = m_grid;
    const AffixLibrary* affixes = m_affixes;
    std::mt19937* rng = m_rng;
    const entt::entity actor_handle = actor.Handle();
    const Vec2 swing_origin = origin;
    const int hits_per_turn = before_attack.hits_per_turn;
    std::vector<RaceBonusEntry> race_bonuses = before_attack.race_bonuses;
    const std::uint32_t status_effect_id = before_attack.status_effect_id;
    const int status_chance_percent = before_attack.status_chance_percent;
    const StatsComponent attacker_stats = before_attack.attacker_stats;
    const std::uint32_t hit_effect_prefab_id = before_attack.hit_effect_prefab_id;
    const float hit_effect_duration = before_attack.hit_effect_duration;
    const int hit_stun_energy = before_attack.hit_stun_energy;

    auto apply_damage = [registry_ptr, grid_ptr, affixes, rng, actor_handle, swing_origin, targets, hits_per_turn,
                         race_bonuses, status_effect_id, status_chance_percent, attacker_stats, hit_effect_prefab_id,
                         hit_effect_duration, hit_stun_energy]()
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

            bool landed_hit = false;
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
                landed_hit = true;

                const int boosted_atp = ApplyRaceBonus(attacker_stats.atp, race_bonuses, defender_race_id);
                int damage = ComputeDamage(boosted_atp, defender_stats.dfp, variance_roll(*rng));
                const bool is_critical = unit_roll(*rng) < ComputeCritChance(attacker_stats.lck);
                damage = ApplyCritical(damage, is_critical);

                BeforeDamageEvent before{target, damage};
                actor.Dispatch(before);
                damage = before.incoming_damage;

                IncomingDamageEvent incoming{actor,       damage, is_critical, hit_effect_prefab_id, hit_effect_duration,
                                            hit_stun_energy};
                target.Dispatch(incoming);

                if (!target.IsValid())
                    break;

                // The weapon's own elemental flavor (if any) gets a chance
                // to inflict its ailment on a landed, non-lethal hit.
                MaybeApplyElementalStatus(target, registry.GetStatusEffectLibrary(), status_effect_id,
                                          status_chance_percent, *rng);
            }

            if (landed_hit)
                ApplyKnockback(registry, *grid_ptr, target_handle, swing_origin);
        }
    };

    const Vec2f peak_offset = Vec2f{static_cast<float>(direction.x), static_cast<float>(direction.y)} * kLungeDistance;
    TweenComponent& tween_component = actor.GetOrEmplace<TweenComponent>();
    tween_component.queue.push_back(Tween{Vec2f{}, peak_offset, kLungeOutDuration, 0.0f, std::move(apply_damage)});
    tween_component.queue.push_back(Tween{peak_offset, Vec2f{}, kLungeBackDuration, 0.0f, nullptr});

    AfterAttackEvent after_attack{true};
    actor.Dispatch(after_attack);

    return ActionResult(EffectiveActCost(actor, kWeaponAttackCost));
}

} // namespace psr
