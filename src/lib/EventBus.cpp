#include "lib/EventBus.h"

#include <algorithm>
#include <vector>

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
    std::erase_if(m_handlers, [id](const auto& p) { return p.first == id; });
}

void EventBus::Publish(GameEvent event)
{
    // Ids only, so a handler may subscribe or unsubscribe mid-dispatch without copying every
    // std::function on every event.
    std::vector<SubscriptionId> snapshot;
    snapshot.reserve(m_handlers.size());
    for (const auto& [id, handler] : m_handlers)
    {
        snapshot.push_back(id);
    }

    for (const SubscriptionId id : snapshot)
    {
        // Looked up each time: a handler removed earlier in this dispatch is skipped, and the
        // vector may have moved.
        const auto it = std::find_if(m_handlers.begin(), m_handlers.end(),
                                     [id](const auto& p) { return p.first == id; });
        if (it == m_handlers.end())
        {
            continue;
        }
        it->second(event);
    }
}

} // namespace ac
