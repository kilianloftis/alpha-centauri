#include "input/PlatformEventQueue.h"

namespace ac
{

void PlatformEventQueue::PushKey(const KeyEvent_t& rEvent)
{
    m_keys.push_back(rEvent);
}

void PlatformEventQueue::PushMouse(const MouseEvent_t& rEvent)
{
    m_lastMousePosition = MousePosition_t{rEvent.x, rEvent.y};
    m_mouseEvents.push_back(rEvent);
}

void PlatformEventQueue::RequestClose()
{
    m_bCloseRequested = true;
}

std::optional<KeyEvent_t> PlatformEventQueue::PopKey()
{
    if (m_keys.empty())
    {
        return std::nullopt;
    }
    const KeyEvent_t event = m_keys.front();
    m_keys.pop_front();
    return event;
}

std::optional<MouseEvent_t> PlatformEventQueue::PopMouse()
{
    if (m_mouseEvents.empty())
    {
        return std::nullopt;
    }
    const MouseEvent_t event = m_mouseEvents.front();
    m_mouseEvents.pop_front();
    return event;
}

bool PlatformEventQueue::TakeCloseRequest()
{
    const bool bRequested = m_bCloseRequested;
    m_bCloseRequested = false;
    return bRequested;
}

std::optional<MousePosition_t> PlatformEventQueue::GetLastMousePosition() const
{
    return m_lastMousePosition;
}

} // namespace ac
