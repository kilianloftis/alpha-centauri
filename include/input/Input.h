#pragma once

#include <functional>
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
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
};

struct KeyEvent_t
{
    Key_t key;
};

enum class Modifier_t
{
    None,
    Ctrl,
    Alt,
    Shift,
};

struct ModifierState_t
{
    bool bCtrl = false;
    bool bAlt = false;
    bool bShift = false;
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

class Input
{
public:
    virtual ~Input() = default;

    virtual bool Initialize() = 0;
    virtual void CaptureKeyAsync(std::function<void(KeyEvent_t)> callback) = 0;
    virtual std::optional<Key_t>CaptureKey() = 0;

    virtual void CaptureMouseAsync(std::function<void(MouseEvent_t)> callback) = 0;
    virtual std::optional<MouseEvent_t> CaptureMouse() = 0;
};

std::unique_ptr<Input> CreateInput();

} // namespace ac
