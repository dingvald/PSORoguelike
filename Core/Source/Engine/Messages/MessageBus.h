#pragma once

#include "Engine/Messages/MessageQueue.h"

#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace psr {

// Thread-safe pub-sub routing table: maps a message type to the
// MessageQueues subscribed to it. Queues are owned by their Layer; the bus
// only holds non-owning pointers to them.
class MessageBus
{
public:
    MessageBus() = default;
    ~MessageBus() = default;

    MessageBus(const MessageBus&) = delete;
    MessageBus& operator=(const MessageBus&) = delete;
    MessageBus(MessageBus&&) = delete;
    MessageBus& operator=(MessageBus&&) = delete;

    template <typename TMessage> void Subscribe(MessageQueue& queue)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_subscribers[std::type_index(typeid(TMessage))].push_back(&queue);
    }

    // Removes queue from every message type's subscriber list.
    void UnsubscribeAll(MessageQueue& queue);

    // Delivers message into every subscriber's queue for TMessage. Delivery
    // is immediate; handler invocation is deferred until each subscriber
    // calls MessageQueue::HandleQueuedMessages().
    template <typename TMessage> void Publish(TMessage message)
    {
        auto data = std::make_shared<const TMessage>(std::move(message));
        const std::type_index type(typeid(TMessage));

        std::vector<MessageQueue*> targets;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_subscribers.find(type);
            if (it != m_subscribers.end())
                targets = it->second;
        }

        for (MessageQueue* queue : targets)
            queue->Enqueue(type, std::static_pointer_cast<const void>(data));
    }

private:
    std::mutex m_mutex;
    std::unordered_map<std::type_index, std::vector<MessageQueue*>> m_subscribers;
};

} // namespace psr
