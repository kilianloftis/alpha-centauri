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
    SubscriptionId Subscribe(Handler handler);

    // Subscribe to a specific event type only.
    template<typename T>
    SubscriptionId Subscribe(std::function<void(const T&)> handler)
    {
        return Subscribe([h = std::move(handler)](const GameEvent& e) {
            if (auto* p = std::get_if<T>(&e)) h(*p);
        });
    }

    void Unsubscribe(SubscriptionId id);

    // Synchronous dispatch. Safe to Subscribe/Unsubscribe from inside a handler: the handler
    // list is snapshotted, one removed mid-dispatch is not called, and one added mid-dispatch
    // does not see the in-flight event.
    void Publish(GameEvent event);

private:
    std::vector<std::pair<SubscriptionId, Handler>> m_handlers;
    SubscriptionId m_nextId = 0;
};

} // namespace ac