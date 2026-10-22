#pragma once

#include "input/Input.h"

#include <deque>
#include <optional>

namespace ac
{

// Between the window pump and Input. The composition root hands one to both, so neither knows
// the other's backend. Pumped and drained on one thread.
class PlatformEventQueue
{
public:
    // Producer side — called by the windowing backend's event pump.
    void PushKey(const KeyEvent_t& rEvent);
    void PushMouse(const MouseEvent_t& rEvent);

    // A window-close request, recorded only: what closing means is the engine's decision.
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
