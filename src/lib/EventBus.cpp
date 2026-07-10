#include "lib/EventBus.h"

#include <algorithm>

namespace ac
{

SubscriptionId EventBus::Subscribe(Handler handler)
{
    auto id = m_nextId++;
    m_handlers.emplace_back(id, std::move(handler));
    return id;
}

void EventBus::Unsubscribe(SubscriptionId id)
{
    m_handlers.erase(
        std::remove_if(m_handlers.begin(), m_handlers.end(),
            [id](const auto& p) { return p.first == id; }),
        m_handlers.end());
}

void EventBus::Publish(GameEvent event)
{
    for (const auto& [_, h] : m_handlers) h(event);
}

} // namespace ac