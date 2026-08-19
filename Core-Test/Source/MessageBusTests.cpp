#include "Engine/Messages/MessageBus.h"
#include "Engine/Messages/MessageQueue.h"

#include <catch2/catch_test_macros.hpp>

namespace {

struct TestMessage
{
    int value;
};

struct OtherMessage
{
    int value;
};

} // namespace

TEST_CASE("MessageBus delivers published messages only to subscribed queues", "[MessageBus]")
{
    psr::MessageBus bus;
    psr::MessageQueue subscriber;
    psr::MessageQueue nonSubscriber;

    int received = 0;
    subscriber.RegisterHandler<TestMessage>([&received](const TestMessage& message) { received = message.value; });

    bus.Subscribe<TestMessage>(subscriber);
    bus.Publish(TestMessage{42});

    subscriber.HandleQueuedMessages();
    nonSubscriber.HandleQueuedMessages();

    REQUIRE(received == 42);
}

TEST_CASE("MessageBus ignores messages of a type nothing subscribed to", "[MessageBus]")
{
    psr::MessageBus bus;
    psr::MessageQueue subscriber;

    bool handlerCalled = false;
    subscriber.RegisterHandler<TestMessage>([&handlerCalled](const TestMessage&) { handlerCalled = true; });

    bus.Subscribe<TestMessage>(subscriber);
    bus.Publish(OtherMessage{1});

    subscriber.HandleQueuedMessages();

    REQUIRE_FALSE(handlerCalled);
}

TEST_CASE("MessageBus stops delivery to a queue after UnsubscribeAll", "[MessageBus]")
{
    psr::MessageBus bus;
    psr::MessageQueue subscriber;

    int callCount = 0;
    subscriber.RegisterHandler<TestMessage>([&callCount](const TestMessage&) { ++callCount; });

    bus.Subscribe<TestMessage>(subscriber);
    bus.Publish(TestMessage{1});
    subscriber.HandleQueuedMessages();

    bus.UnsubscribeAll(subscriber);
    bus.Publish(TestMessage{2});
    subscriber.HandleQueuedMessages();

    REQUIRE(callCount == 1);
}
