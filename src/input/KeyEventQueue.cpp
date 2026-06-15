#include "input/KeyEventQueue.h"
#include <deque>

namespace ac
{

static std::deque<Key_t>g_pendingKeyEvents;

void PushPendingKeyEvent_t(Key_t key)
{
    g_pendingKeyEvents.push_back(key);
}

std::optional<Key_t>PopPendingKeyEvent()
{
if (g_pendingKeyEvents.empty())
    {
        return std::nullopt;
    }

    auto key = g_pendingKeyEvents.front();
    g_pendingKeyEvents.pop_front();
    return key;
}

} // namespace ac
