#pragma once

#include "Actions/WaitAction.h"
#include "Components/AiComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"
#include "Items/AffixLibrary.h"

#include <memory>
#include <random>

namespace psr {

class IAction;

// The AI seam TurnCoordinator::SetNpcDecision expects: Decide(actor) picks
// what a non-player actor with an AiComponent does this turn. Currently only
// AiBehavior::ChaseAndAttack exists -- step toward the nearest
// PlayerControlledComponent entity within AiComponent::detection_range tiles
// (Manhattan distance), one cardinal step at a time, via MoveAction. No
// separate "am I in range, should I attack instead" check exists: MoveAction's
// own bump-into-hostile fallback already turns a step into an adjacent
// hostile's tile into an AttackAction, so chasing into range is attacking in
// range.
//
// Holds a single m_pending_decision, reassigned each Decide() call, to
// satisfy the "IAction* stays valid for at least the Step() call it's
// returned from" contract despite MoveAction having no way to reconfigure an
// existing instance in place (see TurnCoordinator.h's doc comment).
class EnemyAiSystem
{
public:
    EnemyAiSystem(Grid& grid, Registry& registry, const AffixLibrary& affixes, std::mt19937& rng);

    IAction* Decide(Entity actor);

private:
    // Returns nullptr (rather than m_wait_action) when it has nothing to do
    // this turn, so Decide's per-behavior dispatch can uniformly fall back to
    // Wait -- add one of these per new AiBehavior value.
    IAction* DecideChaseAndAttack(Entity actor, const AiComponent& ai);

    Grid* m_grid;
    Registry* m_registry;
    const AffixLibrary* m_affixes;
    std::mt19937* m_rng;
    WaitAction m_wait_action;
    std::unique_ptr<IAction> m_pending_decision;
};

} // namespace psr
