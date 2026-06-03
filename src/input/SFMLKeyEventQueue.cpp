#ifdef USE_SFML

#include "input/SFMLKeyEventQueue.h"
#include <deque>

namespace ac {

static std::deque<Key> g_pendingKeyEvents;

void PushPendingKeyEvent(Key key) {
    g_pendingKeyEvents.push_back(key);
}

std::optional<Key> PopPendingKeyEvent() {
    if (g_pendingKeyEvents.empty()) {
        return std::nullopt;
    }

    auto key = g_pendingKeyEvents.front();
    g_pendingKeyEvents.pop_front();
    return key;
}

} // namespace ac

#endif // USE_SFML
