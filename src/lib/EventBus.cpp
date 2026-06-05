#include "lib/EventBus.h"

namespace smac {

SubscriptionId EventBus::subscribe(Handler handler) {
    auto id = next_id_++;
    handlers_.emplace_back(id, std::move(handler));
    return id;
}

// Subscribe to a specific event type only.
template<typename T>
SubscriptionId EventBus::subscribe(std::function<void(const T&)> handler) {
    return subscribe([h = std::move(handler)](const GameEvent& e) {
            if (auto* p = std::get_if<T>(&e)) h(*p);
        });
}

void EventBus::unsubscribe(SubscriptionId id) {
    handlers_.erase(std::remove_if(handlers_.begin(), handlers_.end(),
        [id](const auto& p) { return p.first == id; }), handlers_.end());
}

void EventBus::publish(GameEvent event) {
    for (const auto& [_, h] : handlers_) h(event);
}

} // namespace smac