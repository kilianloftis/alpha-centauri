#include "input/MouseEventQueue.h"
#include <deque>

namespace ac
{

static std::deque<MouseEvent_t> g_pendingMouseEvents;
static LastMousePosition_t g_lastMousePosition{};
static bool g_bHasLastMousePosition = false;

void PushPendingMouseEvent_t(MouseEvent_t event)
{
    g_lastMousePosition = {event.x, event.y};
    g_bHasLastMousePosition = true;
    g_pendingMouseEvents.push_back(event);
}

bool HasLastMousePosition()
{
    return g_bHasLastMousePosition;
}

LastMousePosition_t GetLastMousePosition()
{
    return g_lastMousePosition;
}

std::optional<MouseEvent_t> PopPendingMouseEvent()
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
