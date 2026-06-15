#include "input/MouseEventQueue.h"
#include <deque>

namespace ac
{

static std::deque<MouseEvent_t> g_pendingMouseEvents;

void PushPendingMouseEvent_t(MouseEvent_t event)
{
    g_pendingMouseEvents.push_back(event);
}

std::optional<MouseEvent_t> PopPendingMouseEvent()
{
    if (g_pendingMouseEvents.empty() || g_pendingMouseEvents.front().button == MouseButton_t::None)
    {
        return std::nullopt;
    }

    auto event = g_pendingMouseEvents.front();
    g_pendingMouseEvents.pop_front();
    return event;
}

} // namespace ac
