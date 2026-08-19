#include "Engine/Messages/MessageQueue.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <typeindex>
#include <vector>

namespace {

struct TestMessage
{
    int value;
};

struct UnhandledMessage
{
    int value;
};

} // namespace

TEST_CASE("MessageQueue dispatches enqueued messages to their registered handler", "[MessageQueue]")
{
    psr::MessageQueue queue;

    std::vector<int> received;
    queue.RegisterHandler<TestMessage>([&received](const TestMessage& message) { received.push_back(message.value); });

    queue.Enqueue(std::type_index(typeid(TestMessage)), std::make_shared<const TestMessage>(TestMessage{1}));
    queue.Enqueue(std::type_index(typeid(TestMessage)), std::make_shared<const TestMessage>(TestMessage{2}));

    queue.HandleQueuedMessages();

    REQUIRE(received == std::vector<int>{1, 2});
}

TEST_CASE("MessageQueue drops messages with no registered handler", "[MessageQueue]")
{
    psr::MessageQueue queue;

    // No crash and no-op expected: HandleQueuedMessages() must silently skip
    // a pending message whose type has no handler registered.
    queue.Enqueue(std::type_index(typeid(UnhandledMessage)), std::make_shared<const UnhandledMessage>(UnhandledMessage{1}));

    REQUIRE_NOTHROW(queue.HandleQueuedMessages());
}

TEST_CASE("MessageQueue clears pending messages after handling them once", "[MessageQueue]")
{
    psr::MessageQueue queue;

    int callCount = 0;
    queue.RegisterHandler<TestMessage>([&callCount](const TestMessage&) { ++callCount; });

    queue.Enqueue(std::type_index(typeid(TestMessage)), std::make_shared<const TestMessage>(TestMessage{1}));
    queue.HandleQueuedMessages();
    queue.HandleQueuedMessages();

    REQUIRE(callCount == 1);
}
