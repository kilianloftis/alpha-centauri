#pragma once

#include <functional>
#include <memory>
#include <optional>

namespace ac
{

enum class Key
{
    Unknown,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Space,
    Escape,
    Enter,
};

struct KeyEvent
{
    Key key;
};

enum class MouseButton
{
    None,
    Left,
    Right,
    Middle,
};

struct MouseEvent
{
    MouseButton button;
    int x;
    int y;
};

class Input
{
public:
    virtual ~Input() = default;

    virtual bool Initialize() = 0;
    virtual void CaptureKeyAsync(std::function<void(KeyEvent)> callback) = 0;
    virtual std::optional<Key> CaptureKey() = 0;

    virtual void CaptureMouseAsync(std::function<void(MouseEvent)> callback) = 0;
    virtual std::optional<MouseEvent> CaptureMouse() = 0;
};

std::unique_ptr<Input> CreateInput();

} // namespace ac
