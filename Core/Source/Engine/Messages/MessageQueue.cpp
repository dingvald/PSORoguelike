#include "Engine/Messages/MessageQueue.h"

namespace psr {

void MessageQueue::Enqueue(std::type_index type, std::shared_ptr<const void> message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.push_back(PendingMessage{type, std::move(message)});
}

void MessageQueue::HandleQueuedMessages()
{
    std::deque<PendingMessage> pending;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        pending.swap(m_pending);
    }

    for (const PendingMessage& message : pending)
    {
        std::function<void(const std::shared_ptr<const void>&)> handler;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_handlers.find(message.type);
            if (it != m_handlers.end())
                handler = it->second;
        }
        if (handler)
            handler(message.data);
    }
}

} // namespace psr
