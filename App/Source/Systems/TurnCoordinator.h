#pragma once

#include "Actions/WaitAction.h"
#include "Components/EnergyComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Input/ActionMap.h"
#include "Engine/Input/InputBuffer.h"
#include "Engine/Turns/TurnQueue.h"

#include <functional>
#include <optional>

namespace psr {

enum class TurnStep
{
    AwaitingInput, // nothing to resolve this call -- no actor is ready, or the player has no pending input
    Resolved,      // the player's action (or a free no-op it attempted) resolved this call
};

// Drives the turn loop: pulls the next-ready actor from a TurnQueue kept in
// sync with every live EnergyComponent, resolves the player's pending input
// or (for now) a placeholder Wait for every other actor, and requeues the
// result. AI is out of scope this round -- SetNpcDecision is the seam a
// future AI session plugs a real decision function into without touching
// this loop.
class TurnCoordinator
{
public:
    explicit TurnCoordinator(Registry& registry, int action_threshold = TurnQueue::kDefaultActionThreshold);
    ~TurnCoordinator();

    // Bound queue-membership listeners capture this instance's address --
    // neither copying nor moving would keep them valid (C.21/C.81).
    TurnCoordinator(const TurnCoordinator&) = delete;
    TurnCoordinator& operator=(const TurnCoordinator&) = delete;
    TurnCoordinator(TurnCoordinator&&) = delete;
    TurnCoordinator& operator=(TurnCoordinator&&) = delete;

    void PressKey(int key_code);
    void ReleaseKey(int key_code);

    ActionMap<int>& KeyBindings() { return m_key_bindings; }

    // AI seam: replaces how non-player actors decide their action (default:
    // every non-player actor Waits). The returned IAction* must stay valid
    // for at least the duration of the Step() call it's returned from.
    void SetNpcDecision(std::function<IAction*(Entity)> decide) { m_decide_npc_action = std::move(decide); }

    // Advances the turn loop by delta_time: ticks in-flight tweens and the
    // input buffer, then lets every non-player actor act before yielding
    // once the player has acted (or the player has no pending input).
    //
    // Requires at least one live PlayerControlledComponent-tagged actor with
    // an EnergyComponent to be queued before this is called: TurnQueue's
    // "time" is turns, not wall-clock seconds, so NextActor() always
    // fast-forwards to *someone* rather than reporting "no one is ready" --
    // this loop only terminates by reaching the player (returning
    // AwaitingInput/Resolved) or finding the queue empty. A queue with only
    // non-player actors in it never yields.
    TurnStep Step(float delta_time);

private:
    void OnEnergyConstructed(entt::registry& registry, entt::entity entity);
    void OnEnergyDestroyed(entt::registry& registry, entt::entity entity);

    Registry* m_registry;
    TurnQueue m_turn_queue;
    ActionMap<int> m_key_bindings;
    InputBuffer<int> m_input_buffer{/*initial_delay_seconds=*/0.3f, /*repeat_interval_seconds=*/0.1f};
    std::optional<int> m_pending_key;
    std::function<IAction*(Entity)> m_decide_npc_action;
    WaitAction m_default_npc_action;
};

} // namespace psr
