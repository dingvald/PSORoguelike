#pragma once

#include <entt/entt.hpp>

#include <cstdint>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace psr {

// Energy based turn scheduler (the classic roguelike time system), generic
// over nothing but the entt::entity handle it schedules -- no
// Registry/Grid/entt::registry dependency, so it's unit-testable in
// isolation (Core-Test). Every actor accrues energy at the same fixed rate
// (1 point per tick) -- all cadence variability comes from how much each
// resolved action costs (the caller's ActionResult), debited via Requeue,
// not from any per-actor rate.
//
// Internally this tracks, per actor, the absolute scheduling time at which
// its energy will next reach action_threshold -- equivalent to (and derived
// from) the same jump math a naive per-call rescan would use, but cached so
// NextActor()/Enqueue()/Remove()/Requeue() are all O(log n) instead of
// O(n). A queued actor's cached "next action time" only changes when that
// actor itself is Requeue()'d; every other actor's value stays valid
// untouched, which is what makes the caching correct.
class TurnQueue
{
public:
    static constexpr int kDefaultActionThreshold = 100;

    explicit TurnQueue(int action_threshold = kDefaultActionThreshold);

    // id must not already be queued.
    void Enqueue(entt::entity id, int initial_energy = 0);

    // id must be queued. Safe to call for the actor most recently returned
    // by NextActor(), even before a matching Requeue().
    void Remove(entt::entity id);

    // Requires at least one queued actor. Idempotent: calling this again
    // before Requeue() for the returned id just returns the same id again,
    // since nothing about that actor's state has changed in between. Ties
    // (multiple actors reaching action_threshold at the same scheduling
    // time) break by insertion order, permanently -- re-acting/Requeue()ing
    // never changes an actor's tie-break priority.
    entt::entity NextActor();

    // id must be queued. remaining_energy replaces id's scheduling energy
    // outright -- it may be negative (an energy debt carries over to the
    // next jump) or still >= action_threshold (a banked surplus left by a
    // cheaper-than-normal action, letting it act again immediately).
    void Requeue(entt::entity id, int remaining_energy);

    // id must be queued. Read-only inspection of id's current scheduling
    // energy -- not used by the scheduling algorithm itself, but necessary
    // for a caller (or a test) to compute the right value to pass to
    // Requeue() after debiting an action's cost, since NextActor() never
    // reports the winner's energy on its own.
    int GetEnergy(entt::entity id) const;

    // Whether id is currently queued. Unlike the other lookups this never
    // asserts on a miss -- it's the safe membership test (e.g. for
    // confirming an actor left the queue after its entity was destroyed).
    bool Contains(entt::entity id) const;

    // Whether the queue currently has no actors at all -- the safe guard
    // before calling NextActor(), which asserts on an empty queue.
    bool IsEmpty() const;

    // Read-only inspection of every queued actor, sorted soonest-to-act
    // first (ties broken by insertion order, same as NextActor()) --
    // debug/inspection only, not used by the scheduling algorithm itself.
    struct Snapshot
    {
        entt::entity id;
        int energy;
        int ticks_to_act;
    };

    std::vector<Snapshot> GetSnapshot() const;

private:
    struct ActorState
    {
        int base_energy;
        std::int64_t base_time;
        int seq;
    };

    // Scheduling key: (absolute time this actor's energy next reaches
    // action_threshold, insertion sequence). Ordering by this pair is what
    // gives NextActor() its soonest-first, ties-by-insertion-order behavior.
    using Key = std::pair<std::int64_t, int>;

    Key MakeKey(const ActorState& state) const;

    int m_action_threshold;
    int m_next_seq = 0;
    std::int64_t m_global_time = 0;
    std::unordered_map<entt::entity, ActorState> m_states;
    std::map<Key, entt::entity> m_order;
};

} // namespace psr
