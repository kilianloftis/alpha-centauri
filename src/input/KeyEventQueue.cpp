#include "input/KeyEventQueue.h"
#include <deque>

namespace ac
{

static std::deque<KeyEvent_t> g_pendingKeyEvents;

void PushPendingKeyEvent_t(const KeyEvent_t& rEvent)
{
    g_pendingKeyEvents.push_back(rEvent);
}

std::optional<KeyEvent_t> PopPendingKeyEvent()
{
    if (g_pendingKeyEvents.empty())
    {
        return std::nullopt;
    }

    const KeyEvent_t event = g_pendingKeyEvents.front();
    g_pendingKeyEvents.pop_front();
    return event;
}

} // namespace ac
