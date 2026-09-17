#pragma once

#include "Actions/WaitAction.h"
#include "Components/AiComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Math/Vec2.h"
#include "Engine/World/Grid.h"
#include "Items/AffixLibrary.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <random>

namespace psr {

class IAction;
class TechniqueLibrary;
class VisualEffectSystem;

// The AI seam TurnCoordinator::SetNpcDecision expects: Decide(actor) picks
// what a non-player actor with an AiComponent does this turn.
//
// - ChaseAndAttack: paths toward the nearest PlayerControlledComponent entity
//   within AiComponent::detection_range tiles (Manhattan distance) that it
//   also has an unobstructed line of sight to (see TargetResolution.h's
//   HasLineOfSight -- a wall blocks detection no matter how close the target
//   is), routing around obstacles via a real A* search (see Pathfinder.h)
//   instead of a naive greedy step -- see StepTowardGoal. No separate "am I
//   in range, should I attack instead" check exists: MoveAction's own
//   bump-into-hostile fallback already turns a step into an adjacent
//   hostile's tile into a WeaponAttackAction, so chasing into range is
//   attacking in range.
// - FleeWhenHit: behaves exactly like ChaseAndAttack until this entity's own
//   HealthComponent shows any damage taken (current_hp < max_hp), then steps
//   directly away from the nearest hostile every turn instead (see
//   StepAwayFrom -- a destination-seeking pathfinder has no notion of
//   "maximize distance from X", so fleeing keeps the old greedy
//   step-and-degrade logic rather than routing through FindPath) -- a
//   simplified stand-in for PSO's Rag Rappy (which additionally never truly
//   dies; that "play dead and revive" mechanic is out of scope here, per the
//   user's explicit choice -- this entity dies normally at 0 HP).
// - StationarySpawner (needs a sibling SpawnerAiComponent): never moves or
//   attacks; ticks SpawnerAiComponent::cooldown_remaining down each turn and,
//   once it reaches zero and fewer than max_alive of its own prior spawns
//   (tracked via SpawnedByComponent) are still alive, spawns one
//   spawn_prefab_id entity into an adjacent open tile via the same
//   on_spawned callback GameplayLayer's own enemy-spawn wiring uses (energy/
//   equipment/combat-log subscriptions) -- mirrors PSO's Monest releasing
//   Mothmants.
// - PackFollower (needs a sibling PackFollowerComponent): behaves exactly
//   like ChaseAndAttack, but each turn also checks whether a living
//   RaceComponent::race_id == pack_leader_race_id entity is within
//   detection_range; the first turn none is found, applies a one-time
//   permanent ATP/DFP penalty to its own StatsComponent (never reapplied) --
//   a simplified stand-in for PSO's Savage Wolves panicking when their
//   Barbarous Wolf pack leader dies (see PackFollowerComponent.h for why
//   pack membership is approximated by race rather than an authored group).
// - RangedTechAtDistance (needs a sibling RangedTechComponent, a
//   TPComponent, and a KnownTechniquesComponent entry granted at spawn --
//   see GameplayLayer's on_enemy_spawned): melees when adjacent (same
//   bump-fallback as ChaseAndAttack); when the target is aligned on a
//   cardinal row/column within RangedTechComponent::range, casts
//   RangedTechComponent::technique_id at it instead (writing
//   SelectedTargetComponent itself, the same way the player's interactive
//   target-confirm flow does, then returning a TechniqueAction); otherwise
//   closes distance like ChaseAndAttack. Mirrors PSO's Hildebear stopping to
//   cast Foie at range instead of always closing to melee.
// - KeepDistanceAndPounce (needs a sibling PounceComponent): a kiting
//   predator rather than a straight chaser. Retreats (StepAwayFrom) once the
//   target closes to PounceComponent::preferred_distance - 1 tiles or
//   nearer (Chebyshev), approaches (StepTowardGoal) once the target opens
//   past preferred_distance, and at exactly preferred_distance -- with the
//   target 8-directionally aligned (cardinal or diagonal) -- rolls
//   PounceComponent::pounce_chance_percent each turn to either lunge (see
//   LungeAttackAction; preferred_distance must equal
//   LungeAttackAction::kLungeRange for the pounce to ever trigger) or strafe
//   laterally (StepLaterally) instead. Falls back to DecideChaseAndAttack if
//   the sibling component is missing, same convention
//   DecideRangedTechAtDistance uses.
//
// Holds a single m_pending_decision, reassigned each Decide() call, to
// satisfy the "IAction* stays valid for at least the Step() call it's
// returned from" contract despite MoveAction/TechniqueAction having no way to
// reconfigure an existing instance in place (see TurnCoordinator.h's doc
// comment).
class EnemyAiSystem
{
public:
    EnemyAiSystem(Grid& grid, Registry& registry, const AffixLibrary& affixes, const TechniqueLibrary& techniques,
                  VisualEffectSystem& visual_effects, std::mt19937& rng,
                  std::function<void(entt::entity)> on_spawned = {});

    IAction* Decide(Entity actor);

private:
    // Returns nullptr (rather than m_wait_action) when it has nothing to do
    // this turn, so Decide's per-behavior dispatch can uniformly fall back to
    // Wait -- add one of these per new AiBehavior value.
    IAction* DecideChaseAndAttack(Entity actor, const AiComponent& ai);
    IAction* DecideFleeWhenHit(Entity actor, const AiComponent& ai);
    IAction* DecideStationarySpawner(Entity actor);
    IAction* DecidePackFollower(Entity actor, const AiComponent& ai);
    IAction* DecideRangedTechAtDistance(Entity actor, const AiComponent& ai);
    IAction* DecideKeepDistanceAndPounce(Entity actor, const AiComponent& ai);

    // Shared by ChaseAndAttack/PackFollower/FleeWhenHit's approach phase and
    // RangedTechAtDistance's/KeepDistanceAndPounce's close-the-distance
    // fallback: routes actor from self_tile to goal_tile via FindPath (see
    // Pathfinder.h) against TargetResolution.h's IsWalkableStep, and issues a
    // MoveAction for the path's first step. nullptr if goal_tile is
    // unreachable.
    IAction* StepTowardGoal(Entity actor, Vec2 self_tile, Vec2 goal_tile);

    // FleeWhenHit's fleeing phase and KeepDistanceAndPounce's retreat phase,
    // since a destination-seeking pathfinder has no way to "maximize distance
    // from X": one step (diagonal included) from self_tile toward
    // self_tile+delta, degrading to a single cardinal axis (dominant delta
    // first) if the diagonal is blocked. Callers pass delta already pointing
    // away from whatever should be fled (e.g. self_tile - target_tile), not
    // toward it.
    IAction* StepAwayFrom(Entity actor, Vec2 self_tile, Vec2 delta);

    // KeepDistanceAndPounce's steady-state phase: one step perpendicular to
    // the direction toward target_tile (rotating the sign-based unit vector
    // 90 degrees either way), so distance to target_tile is preserved rather
    // than closed or opened. Which of the two perpendiculars is tried first
    // is randomized via m_rng (an unpredictable circling direction, not a
    // fixed one); degrades to the other side if the first is blocked, else
    // nullptr.
    IAction* StepLaterally(Entity actor, Vec2 self_tile, Vec2 target_tile);

    Grid* m_grid;
    Registry* m_registry;
    const AffixLibrary* m_affixes;
    const TechniqueLibrary* m_techniques;
    VisualEffectSystem* m_visual_effects;
    std::mt19937* m_rng;
    std::function<void(entt::entity)> m_on_spawned;
    WaitAction m_wait_action;
    std::unique_ptr<IAction> m_pending_decision;
};

} // namespace psr
