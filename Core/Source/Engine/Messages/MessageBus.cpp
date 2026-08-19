#include "Engine/Messages/MessageBus.h"

#include <vector>

namespace psr {

void MessageBus::UnsubscribeAll(MessageQueue& queue)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [type, subscribers] : m_subscribers)
        std::erase(subscribers, &queue);
}

} // namespace psr
