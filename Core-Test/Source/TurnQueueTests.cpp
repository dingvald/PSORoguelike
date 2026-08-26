#include "Engine/Turns/TurnQueue.h"

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr entt::entity kActorA = static_cast<entt::entity>(1);
constexpr entt::entity kActorB = static_cast<entt::entity>(2);

} // namespace

TEST_CASE("TurnQueue returns an actor immediately when its initial energy already meets the threshold", "[TurnQueue]")
{
    psr::TurnQueue queue(/*action_threshold=*/100);
    queue.Enqueue(kActorA, /*initial_energy=*/100);

    REQUIRE(queue.NextActor() == kActorA);
}

TEST_CASE("TurnQueue advances an actor to the action threshold in one jump", "[TurnQueue]")
{
    psr::TurnQueue queue(/*action_threshold=*/100);
    queue.Enqueue(kActorA, /*initial_energy=*/0);

    REQUIRE(queue.NextActor() == kActorA);
}

TEST_CASE("TurnQueue lets a cheaper-acting actor act twice for every one turn a normal-cost actor gets", "[TurnQueue]")
{
    psr::TurnQueue queue(/*action_threshold=*/100);
    queue.Enqueue(kActorA, /*initial_energy=*/0);
    queue.Enqueue(kActorB, /*initial_energy=*/0);

    constexpr int kNormalCost = 100;
    constexpr int kHalfCost = 50;
    int a_turns = 0;
    int b_turns = 0;
    for (int i = 0; i < 300; ++i)
    {
        entt::entity actor = queue.NextActor();
        int cost = (actor == kActorA) ? kNormalCost : kHalfCost;
        if (actor == kActorA)
            ++a_turns;
        else
            ++b_turns;

        // Debit the action's cost from whatever the winner is actually
        // carrying (its banked surplus energy from a cheap prior action is
        // exactly what lets it act again before the other actor is ready)
        // and requeue with what's left -- mirrors how a real caller would
        // debit an actor's energy and report it back.
        queue.Requeue(actor, queue.GetEnergy(actor) - cost);
    }

    REQUIRE(b_turns == 2 * a_turns);
}

TEST_CASE("TurnQueue breaks ties between equally-ready actors by insertion order", "[TurnQueue]")
{
    psr::TurnQueue queue(/*action_threshold=*/100);
    queue.Enqueue(kActorA, /*initial_energy=*/0);
    queue.Enqueue(kActorB, /*initial_energy=*/0);

    REQUIRE(queue.NextActor() == kActorA);
}

TEST_CASE("TurnQueue Requeue with negative remaining energy delays the actor's next turn by more than one jump",
          "[TurnQueue]")
{
    psr::TurnQueue queue(/*action_threshold=*/100);
    queue.Enqueue(kActorA, /*initial_energy=*/0);
    queue.Enqueue(kActorB, /*initial_energy=*/0);

    // Both actors reach the threshold on the same jump; insertion order
    // breaks the tie in A's favor.
    REQUIRE(queue.NextActor() == kActorA);
    queue.Requeue(kActorA, /*remaining_energy=*/-200);

    // The jump that just made A ready also left B banked at exactly the
    // threshold (untouched since B wasn't the winner) -- B's very next call
    // needs no jump at all and wins for free. Two more jumps then bring A's
    // energy from -200 up through -100 and 0 (B re-winning each time on its
    // own single-jump cadence) before a shared jump finally brings both back
    // to exactly the threshold together, where insertion order hands the
    // turn back to A.
    REQUIRE(queue.NextActor() == kActorB);
    queue.Requeue(kActorB, 0);

    REQUIRE(queue.NextActor() == kActorB);
    queue.Requeue(kActorB, 0);

    REQUIRE(queue.NextActor() == kActorB);
    queue.Requeue(kActorB, 0);

    REQUIRE(queue.NextActor() == kActorA);
}

TEST_CASE("TurnQueue NextActor is idempotent without an intervening Requeue", "[TurnQueue]")
{
    psr::TurnQueue queue(/*action_threshold=*/100);
    queue.Enqueue(kActorA, /*initial_energy=*/0);

    entt::entity first = queue.NextActor();
    entt::entity second = queue.NextActor();

    REQUIRE(first == kActorA);
    REQUIRE(second == kActorA);
}

TEST_CASE("TurnQueue Remove drops an actor so it's never returned again", "[TurnQueue]")
{
    psr::TurnQueue queue(/*action_threshold=*/100);
    queue.Enqueue(kActorA, /*initial_energy=*/0);
    queue.Enqueue(kActorB, /*initial_energy=*/0);

    queue.Remove(kActorA);

    for (int i = 0; i < 5; ++i)
    {
        entt::entity actor = queue.NextActor();
        REQUIRE(actor == kActorB);
        queue.Requeue(actor, 0);
    }
}

TEST_CASE("TurnQueue Remove on the actor most recently returned by NextActor leaves the queue consistent",
          "[TurnQueue]")
{
    psr::TurnQueue queue(/*action_threshold=*/100);
    queue.Enqueue(kActorA, /*initial_energy=*/0);
    queue.Enqueue(kActorB, /*initial_energy=*/0);

    entt::entity winner = queue.NextActor();
    REQUIRE(winner == kActorA);

    queue.Remove(winner);

    REQUIRE(queue.NextActor() == kActorB);
}

TEST_CASE("TurnQueue GetSnapshot reports every queued actor soonest-to-act first with matching energy",
          "[TurnQueue]")
{
    psr::TurnQueue queue(/*action_threshold=*/100);
    queue.Enqueue(kActorA, /*initial_energy=*/50);
    queue.Enqueue(kActorB, /*initial_energy=*/0);

    // Every actor accrues at the same fixed rate, so readiness is purely a
    // function of energy: A (starting at 50) needs only 50 more ticks to
    // reach the threshold, while B (starting at 0) needs 100 -- A sorts
    // first without any insertion-order tie-break being involved.
    std::vector<psr::TurnQueue::Snapshot> snapshot = queue.GetSnapshot();
    REQUIRE(snapshot.size() == 2);

    REQUIRE(snapshot[0].id == kActorA);
    REQUIRE(snapshot[0].energy == 50);
    REQUIRE(snapshot[0].ticks_to_act == 50);

    REQUIRE(snapshot[1].id == kActorB);
    REQUIRE(snapshot[1].energy == 0);
    REQUIRE(snapshot[1].ticks_to_act == 100);

    // A's turn comes up first (advancing global_time to 50); a costly action
    // leaves it at -50 energy on Requeue. That should push it later in a
    // subsequent snapshot, without disturbing B's reported values.
    REQUIRE(queue.NextActor() == kActorA);
    queue.Requeue(kActorA, /*remaining_energy=*/-50);

    snapshot = queue.GetSnapshot();
    REQUIRE(snapshot[0].id == kActorB);
    REQUIRE(snapshot[0].energy == 50);
    REQUIRE(snapshot[0].ticks_to_act == 50);
    REQUIRE(snapshot[1].id == kActorA);
    REQUIRE(snapshot[1].energy == -50);
    REQUIRE(snapshot[1].ticks_to_act == 150);
}

TEST_CASE("TurnQueue Contains reflects membership", "[TurnQueue]")
{
    psr::TurnQueue queue(/*action_threshold=*/100);
    queue.Enqueue(kActorA, /*initial_energy=*/0);

    REQUIRE(queue.Contains(kActorA));
    REQUIRE_FALSE(queue.Contains(kActorB));

    queue.Remove(kActorA);
    REQUIRE_FALSE(queue.Contains(kActorA));
}

TEST_CASE("TurnQueue IsEmpty reflects whether any actor is queued", "[TurnQueue]")
{
    psr::TurnQueue queue(/*action_threshold=*/100);
    REQUIRE(queue.IsEmpty());

    queue.Enqueue(kActorA, /*initial_energy=*/0);
    REQUIRE_FALSE(queue.IsEmpty());

    queue.Remove(kActorA);
    REQUIRE(queue.IsEmpty());
}
