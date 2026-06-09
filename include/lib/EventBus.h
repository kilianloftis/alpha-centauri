#pragma once

#include "lib/GameEvent.h"
#include <functional>
#include <vector>

namespace ac
{

using SubscriptionId = int;

class EventBus
{
public:
    using Handler = std::function<void(const GameEvent&)>;

    // Subscribe to all events (mod-friendly: one handler, switch inside).
    SubscriptionId subscribe(Handler handler);

    // Subscribe to a specific event type only.
    template<typename T>
    SubscriptionId subscribe(std::function<void(const T&)> handler)
    {
        return subscribe([h = std::move(handler)](const GameEvent& e) {
            if (auto* p = std::get_if<T>(&e)) h(*p);
        });
    }

    void unsubscribe(SubscriptionId id);
    void publish(GameEvent event);   // synchronous dispatch

private:
    std::vector<std::pair<SubscriptionId, Handler>> m_handlers;
    SubscriptionId m_nextId = 0;
};

} // namespace ac