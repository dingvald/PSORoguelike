#include "Engine/ECS/EventHandlerComponent.h"

#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace {

struct ProbeEvent
{
    int value = 0;
};

struct OwnerA
{
};
struct OwnerB
{
};

} // namespace

TEST_CASE("EventHandlerComponent Dispatch invokes a subscribed handler with the event by reference",
          "[EventHandlerComponent]")
{
    psr::Registry registry;
    entt::entity handle = registry.CreateEntity();
    psr::Entity entity(registry, handle);

    bool called = false;
    entity.Get<psr::EventHandlerComponent>().Subscribe<ProbeEvent, OwnerA>(
        [&called, handle](psr::Entity self, ProbeEvent& event) {
            called = true;
            event.value += 1;
            REQUIRE(self.Handle() == handle);
        });

    ProbeEvent event{10};
    entity.Dispatch(event);

    REQUIRE(called);
    REQUIRE(event.value == 11);
}

TEST_CASE("EventHandlerComponent Dispatch is a no-op when nothing is subscribed to that event type",
          "[EventHandlerComponent]")
{
    psr::Registry registry;
    psr::Entity entity(registry, registry.CreateEntity());

    ProbeEvent event{5};
    entity.Dispatch(event); // should not throw/crash

    REQUIRE(event.value == 5);
}

TEST_CASE("EventHandlerComponent Unsubscribe removes only the named owner's handler", "[EventHandlerComponent]")
{
    psr::Registry registry;
    psr::Entity entity(registry, registry.CreateEntity());
    psr::EventHandlerComponent& handler = entity.Get<psr::EventHandlerComponent>();

    int a_calls = 0;
    int b_calls = 0;
    handler.Subscribe<ProbeEvent, OwnerA>([&a_calls](psr::Entity, ProbeEvent&) { ++a_calls; });
    handler.Subscribe<ProbeEvent, OwnerB>([&b_calls](psr::Entity, ProbeEvent&) { ++b_calls; });

    handler.Unsubscribe<ProbeEvent, OwnerA>();

    ProbeEvent event{0};
    entity.Dispatch(event);

    REQUIRE(a_calls == 0);
    REQUIRE(b_calls == 1);
}

TEST_CASE("EventHandlerComponent Dispatch runs handlers in ascending priority order regardless of subscribe order",
          "[EventHandlerComponent]")
{
    psr::Registry registry;
    psr::Entity entity(registry, registry.CreateEntity());
    psr::EventHandlerComponent& handler = entity.Get<psr::EventHandlerComponent>();

    std::vector<int> order;
    handler.Subscribe<ProbeEvent, OwnerB>([&order](psr::Entity, ProbeEvent&) { order.push_back(100); }, 100);
    handler.Subscribe<ProbeEvent, OwnerA>([&order](psr::Entity, ProbeEvent&) { order.push_back(0); }, 0);

    ProbeEvent event{0};
    entity.Dispatch(event);

    REQUIRE(order == std::vector<int>{0, 100});
}
