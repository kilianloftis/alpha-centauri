#ifdef USE_SFML

#include "input/Input.h"
#include "input/KeyMapping.h"
#include "input/KeyEventQueue.h"
#include "input/MouseEventQueue.h"
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

namespace ac
{
class SFMLInput : public Input
{
bool Initialize() override
{
    std::cout << "[Input] SFML input backend selected.\n";
    return true;
}

std::optional<Key_t>CaptureKey() override
{
    return PopPendingKeyEvent();
}

void CaptureKeyAsync(std::function<void(KeyEvent_t)> callback) override
{
    if (auto key = CaptureKey())
    {
        callback(KeyEvent_t{*key});
    }
}

std::optional<MouseEvent_t> CaptureMouse() override
{
    return PopPendingMouseEvent();
}

void CaptureMouseAsync(std::function<void(MouseEvent_t)> callback) override
{
    if (auto m = CaptureMouse())
    {
        callback(*m);
    }
}
};

std::unique_ptr<Input> CreateInput()
{
    return std::make_unique<SFMLInput>();
}

} // namespace ac

#endif // USE_SFML
