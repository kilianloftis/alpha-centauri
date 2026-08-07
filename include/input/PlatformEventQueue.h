#pragma once

#include "input/Input.h"

#include <deque>
#include <optional>

namespace ac
{

// The seam between whatever pumps a window and whatever implements Input. The composition root
// owns one and hands it to both backends, so a windowing backend never has to know which Input
// implementation is in play, and neither has to be the SFML one.
//
// Not thread-safe: pumped and drained from the same frame loop.
class PlatformEventQueue
{
public:
    // Producer side — called by the windowing backend's event pump.
    void PushKey(const KeyEvent_t& rEvent);
    void PushMouse(const MouseEvent_t& rEvent);

    // A window-close request. Recorded rather than acted on: what closing means is the
    // engine's decision, not the graphics backend's.
    void RequestClose();

    // Consumer side — called by Input and by the frame loop.
    std::optional<KeyEvent_t> PopKey();
    std::optional<MouseEvent_t> PopMouse();
    bool TakeCloseRequest();

    // Where the mouse was last seen, for consumers that need a position without an event
    // (edge-scrolling). Empty until the first mouse event arrives.
    std::optional<MousePosition_t> GetLastMousePosition() const;

private:
    std::deque<KeyEvent_t> m_keys;
    std::deque<MouseEvent_t> m_mouseEvents;
    std::optional<MousePosition_t> m_lastMousePosition;
    bool m_bCloseRequested = false;
};

} // namespace ac
