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

// The AI seam TurnCoordinator::SetNpcDecision expects: Decide(actor) picks
// what a non-player actor with an AiComponent does this turn.
//
// - ChaseAndAttack: step toward the nearest PlayerControlledComponent entity
//   within AiComponent::detection_range tiles (Manhattan distance), one
//   cardinal step at a time, via MoveAction. No separate "am I in range,
//   should I attack instead" check exists: MoveAction's own bump-into-hostile
//   fallback already turns a step into an adjacent hostile's tile into an
//   AttackAction, so chasing into range is attacking in range.
// - FleeWhenHit: behaves exactly like ChaseAndAttack until this entity's own
//   HealthComponent shows any damage taken (current_hp < max_hp), then steps
//   directly away from the nearest hostile every turn instead -- a
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
                  std::mt19937& rng, std::function<void(entt::entity)> on_spawned = {});

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

    // Shared by ChaseAndAttack/PackFollower/FleeWhenHit's approach phase and
    // RangedTechAtDistance's close-the-distance fallback: one cardinal step
    // from self_tile toward self_tile+delta, retrying the perpendicular axis
    // if the primary one is blocked. Passing -delta instead steps away
    // (FleeWhenHit's fleeing phase).
    IAction* StepToward(Entity actor, Vec2 self_tile, Vec2 delta);

    Grid* m_grid;
    Registry* m_registry;
    const AffixLibrary* m_affixes;
    const TechniqueLibrary* m_techniques;
    std::mt19937* m_rng;
    std::function<void(entt::entity)> m_on_spawned;
    WaitAction m_wait_action;
    std::unique_ptr<IAction> m_pending_decision;
};

} // namespace psr
