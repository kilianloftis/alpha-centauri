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
    m_handlers.erase(
        std::remove_if(m_handlers.begin(), m_handlers.end(),
            [id](const auto& p) { return p.first == id; }),
        m_handlers.end());
}

void EventBus::Publish(GameEvent event)
{
    // Snapshot of ids only, so a handler that subscribes or unsubscribes during dispatch cannot
    // invalidate the iteration. Copying the handlers themselves would allocate per std::function
    // on every event, and this runs per game event with every mod subscribed.
    std::vector<SubscriptionId> snapshot;
    snapshot.reserve(m_handlers.size());
    for (const auto& [id, handler] : m_handlers)
    {
        snapshot.push_back(id);
    }

    for (const SubscriptionId id : snapshot)
    {
        // Re-looked up rather than held: a handler removed earlier in this dispatch must not be
        // invoked, and the vector may have moved.
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
