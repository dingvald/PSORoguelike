#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>

namespace psr {

// Thread-safe inbox owned by a single Layer. Publishers (any thread) call
// Enqueue(); the owning layer drains and dispatches pending messages by
// calling HandleQueuedMessages() whenever it is ready (its own OnUpdate(),
// or a thread it manages itself).
class MessageQueue
{
public:
    MessageQueue() = default;
    ~MessageQueue() = default;

    MessageQueue(const MessageQueue&) = delete;
    MessageQueue& operator=(const MessageQueue&) = delete;
    MessageQueue(MessageQueue&&) = delete;
    MessageQueue& operator=(MessageQueue&&) = delete;

    // Registers the handler invoked for messages of type TMessage during
    // HandleQueuedMessages(). Replaces any handler previously registered
    // for TMessage on this queue.
    template <typename TMessage> void RegisterHandler(std::function<void(const TMessage&)> handler)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handlers[std::type_index(typeid(TMessage))] =
            [handler = std::move(handler)](const std::shared_ptr<const void>& message)
        { handler(*static_cast<const TMessage*>(message.get())); };
    }

    void Enqueue(std::type_index type, std::shared_ptr<const void> message);
    void HandleQueuedMessages();

private:
    struct PendingMessage
    {
        std::type_index type;
        std::shared_ptr<const void> data;
    };

    std::mutex m_mutex;
    std::deque<PendingMessage> m_pending;
    std::unordered_map<std::type_index, std::function<void(const std::shared_ptr<const void>&)>> m_handlers;
};

} // namespace psr
