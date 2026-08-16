#pragma once

#include <memory>
#include <optional>

namespace ac
{

enum class Key_t
{
    Unknown,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Space,
    Escape,
    Enter,
    Backspace,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
};

struct ModifierState_t
{
    bool bCtrl = false;
    bool bAlt = false;
    bool bShift = false;
};

struct KeyEvent_t
{
    Key_t key;
    // Captured with the keystroke by the backend, as MouseEvent_t does. Consumers must read
    // it from the event rather than polling the keyboard, which is SFML-only.
    ModifierState_t modifier;
};

enum class MouseButton_t
{
    None,
    Left,
    Right,
    Middle,
};

struct MouseEvent_t
{
    MouseButton_t button;
    int x;
    int y;
    ModifierState_t modifier;
    bool bPressed = true;
};

struct MousePosition_t
{
    int x = 0;
    int y = 0;
};

// Poll-only, and never blocking: one buffered event per call, or nullopt when the queue is
// empty. Callers drain in a loop. Both are whole events — a key without its modifiers cannot
// express a chord, and KeyEvent_t's contract is that consumers read modifiers from the event.
class Input
{
public:
    virtual ~Input() = default;

    virtual std::optional<KeyEvent_t> PollKey() = 0;
    virtual std::optional<MouseEvent_t> PollMouse() = 0;

    // Where the pointer was last seen, for consumers that need a position rather than an event
    // (edge-scrolling). Empty until the backend has observed one.
    virtual std::optional<MousePosition_t> GetLastMousePosition() const = 0;
};

class PlatformEventQueue;

// rEvents is the queue the windowing backend writes into; the composition root owns it and
// passes the same one to CreateGraphics.
std::unique_ptr<Input> CreateInput(PlatformEventQueue& rEvents);

} // namespace ac
