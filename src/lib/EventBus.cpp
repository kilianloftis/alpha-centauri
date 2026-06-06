#include "lib/EventBus.h"

#include <algorithm>

namespace ac
{

SubscriptionId EventBus::subscribe(Handler handler)
{
    auto id = m_nextId++;
    m_handlers.emplace_back(id, std::move(handler));
    return id;
}

template<typename T>
SubscriptionId EventBus::subscribe(std::function<void(const T&)> handler)
{
    return subscribe([h = std::move(handler)](const GameEvent& e) {
        if (auto* p = std::get_if<T>(&e)) h(*p);
    });
}

void EventBus::unsubscribe(SubscriptionId id)
{
    m_handlers.erase(
        std::remove_if(m_handlers.begin(), m_handlers.end(),
            [id](const auto& p) { return p.first == id; }),
        m_handlers.end());
}

void EventBus::publish(GameEvent event)
{
    for (const auto& [_, h] : m_handlers) h(event);
}

} // namespace ac