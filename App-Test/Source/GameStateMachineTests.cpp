#include "States/GameStateMachine.h"

#include "Engine/ECS/Registry.h"
#include "Engine/Events/Event.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/World/Grid.h"
#include "Systems/TurnCoordinator.h"

#include <catch2/catch_test_macros.hpp>

namespace {

// A minimal GameState that records lifecycle calls and returns whatever
// transition the test configures -- independent of any concrete gameplay
// state (ExploringState/TargetSelectionState have their own dedicated
// tests), exercising GameStateMachine's push/pop/replace/dispatch mechanics
// in isolation.
class FakeState : public psr::GameState
{
public:
    explicit FakeState(psr::GameStateId id) : m_id(id) {}

    psr::GameStateId GetId() const override { return m_id; }

    void OnEnter(psr::GameplayContext& /*context*/) override { ++enter_count; }
    void OnExit(psr::GameplayContext& /*context*/) override { ++exit_count; }

    psr::StateTransition Update(psr::GameplayContext& /*context*/, float /*delta_time*/) override
    {
        ++update_count;
        return next_transition;
    }

    bool HandleEvent(psr::Event& /*event*/, psr::GameplayContext& /*context*/) override
    {
        ++handle_event_count;
        return handled_result;
    }

    psr::GameStateId m_id;
    int enter_count = 0;
    int exit_count = 0;
    int update_count = 0;
    int handle_event_count = 0;
    bool handled_result = false;
    psr::StateTransition next_transition = psr::StateTransition::None();
};

struct Fixture
{
    psr::Registry registry;
    psr::Grid grid{5, 5};
    psr::TurnCoordinator turn_coordinator{registry};
    psr::MessageBus message_bus;
    entt::entity player = entt::null;

    Fixture() { player = registry.CreateEntity(); }

    psr::GameplayContext Context() { return psr::GameplayContext{registry, grid, turn_coordinator, player, message_bus}; }
};

} // namespace

TEST_CASE("GameStateMachine starts empty", "[GameStateMachine]")
{
    psr::GameStateMachine machine;
    CHECK(machine.IsEmpty());
    CHECK(machine.Top() == nullptr);
}

TEST_CASE("GameStateMachine Push fires OnEnter and makes the state the top", "[GameStateMachine]")
{
    Fixture fixture;
    psr::GameStateMachine machine;
    FakeState state(psr::GameStateId::Exploring);
    psr::GameplayContext context = fixture.Context();

    machine.Push(state, context);

    CHECK_FALSE(machine.IsEmpty());
    CHECK(machine.Top() == &state);
    CHECK(state.enter_count == 1);
}

TEST_CASE("GameStateMachine Pop fires OnExit and empties the stack", "[GameStateMachine]")
{
    Fixture fixture;
    psr::GameStateMachine machine;
    FakeState state(psr::GameStateId::Exploring);
    psr::GameplayContext context = fixture.Context();

    machine.Push(state, context);
    machine.Pop(context);

    CHECK(machine.IsEmpty());
    CHECK(state.exit_count == 1);
}

TEST_CASE("GameStateMachine Pop on an empty stack is a no-op", "[GameStateMachine]")
{
    Fixture fixture;
    psr::GameStateMachine machine;
    psr::GameplayContext context = fixture.Context();

    machine.Pop(context); // must not crash

    CHECK(machine.IsEmpty());
}

TEST_CASE("GameStateMachine Replace exits the old top and enters the new one", "[GameStateMachine]")
{
    Fixture fixture;
    psr::GameStateMachine machine;
    FakeState first(psr::GameStateId::Exploring);
    FakeState second(psr::GameStateId::TargetSelection);
    psr::GameplayContext context = fixture.Context();

    machine.Push(first, context);
    machine.Replace(second, context);

    CHECK(first.exit_count == 1);
    CHECK(second.enter_count == 1);
    CHECK(machine.Top() == &second);
}

TEST_CASE("GameStateMachine Update only drives the top state and applies its transition", "[GameStateMachine]")
{
    Fixture fixture;
    psr::GameStateMachine machine;
    FakeState bottom(psr::GameStateId::Exploring);
    FakeState top(psr::GameStateId::TargetSelection);
    psr::GameplayContext context = fixture.Context();

    machine.Push(bottom, context);
    machine.Push(top, context);

    top.next_transition = psr::StateTransition::None();
    machine.Update(context, 0.016f);

    CHECK(top.update_count == 1);
    CHECK(bottom.update_count == 0); // only the top state is driven
}

TEST_CASE("GameStateMachine Update applies a Pop transition returned by the top state", "[GameStateMachine]")
{
    Fixture fixture;
    psr::GameStateMachine machine;
    FakeState state(psr::GameStateId::TargetSelection);
    psr::GameplayContext context = fixture.Context();

    machine.Push(state, context);
    state.next_transition = psr::StateTransition::Pop();
    machine.Update(context, 0.016f);

    CHECK(machine.IsEmpty());
    CHECK(state.exit_count == 1);
}

TEST_CASE("GameStateMachine Update applies a Push transition returned by the top state", "[GameStateMachine]")
{
    Fixture fixture;
    psr::GameStateMachine machine;
    FakeState bottom(psr::GameStateId::Exploring);
    FakeState pushed(psr::GameStateId::TargetSelection);
    psr::GameplayContext context = fixture.Context();

    machine.Push(bottom, context);
    bottom.next_transition = psr::StateTransition::Push(pushed);
    machine.Update(context, 0.016f);

    CHECK(machine.Top() == &pushed);
    CHECK(pushed.enter_count == 1);
}

TEST_CASE("GameStateMachine Update on an empty stack is a no-op", "[GameStateMachine]")
{
    Fixture fixture;
    psr::GameStateMachine machine;
    psr::GameplayContext context = fixture.Context();

    machine.Update(context, 0.016f); // must not crash

    CHECK(machine.IsEmpty());
}

TEST_CASE("GameStateMachine HandleEvent forwards to the top state and returns its result", "[GameStateMachine]")
{
    Fixture fixture;
    psr::GameStateMachine machine;
    FakeState bottom(psr::GameStateId::Exploring);
    FakeState top(psr::GameStateId::TargetSelection);
    psr::GameplayContext context = fixture.Context();

    machine.Push(bottom, context);
    machine.Push(top, context);
    top.handled_result = true;

    class DummyEvent : public psr::Event
    {
    public:
        psr::EventType GetEventType() const override { return psr::EventType::None; }
        const char* GetName() const override { return "Dummy"; }
        int GetCategoryFlags() const override { return psr::EventCategoryNone; }
    } event;

    const bool handled = machine.HandleEvent(event, context);

    CHECK(handled);
    CHECK(top.handle_event_count == 1);
    CHECK(bottom.handle_event_count == 0);
}

TEST_CASE("GameStateMachine HandleEvent on an empty stack returns false", "[GameStateMachine]")
{
    Fixture fixture;
    psr::GameStateMachine machine;
    psr::GameplayContext context = fixture.Context();

    class DummyEvent : public psr::Event
    {
    public:
        psr::EventType GetEventType() const override { return psr::EventType::None; }
        const char* GetName() const override { return "Dummy"; }
        int GetCategoryFlags() const override { return psr::EventCategoryNone; }
    } event;

    CHECK_FALSE(machine.HandleEvent(event, context));
}
