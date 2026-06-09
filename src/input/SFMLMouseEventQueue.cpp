#ifdef USE_SFML

#include "input/SFMLMouseEventQueue.h"
#include <deque>

namespace ac
{

static std::deque<MouseEvent> g_pendingMouseEvents;

void PushPendingMouseEvent(MouseEvent event)
{
    g_pendingMouseEvents.push_back(event);
}

std::optional<MouseEvent> PopPendingMouseEvent()
{
    if (g_pendingMouseEvents.empty())
    {
        return std::nullopt;
    }

    auto event = g_pendingMouseEvents.front();
    g_pendingMouseEvents.pop_front();
    return event;
}

} // namespace ac

#endif // USE_SFML
