#ifdef USE_SFML

#include "input/Input.h"
#include "input/KeyMapping.h"
#include "input/SFMLKeyEventQueue.h"
#include "input/SFMLMouseEventQueue.h"
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

namespace ac
{
static const std::vector<sf::Keyboard::Key> Key_ts =
{
    sf::Keyboard::Key::A, sf::Keyboard::Key::B, sf::Keyboard::Key::C, sf::Keyboard::Key::D,
    sf::Keyboard::Key::E, sf::Keyboard::Key::F, sf::Keyboard::Key::G, sf::Keyboard::Key::H,
    sf::Keyboard::Key::I, sf::Keyboard::Key::J, sf::Keyboard::Key::K, sf::Keyboard::Key::L,
    sf::Keyboard::Key::M, sf::Keyboard::Key::N, sf::Keyboard::Key::O, sf::Keyboard::Key::P,
    sf::Keyboard::Key::Q, sf::Keyboard::Key::R, sf::Keyboard::Key::S, sf::Keyboard::Key::T,
    sf::Keyboard::Key::U, sf::Keyboard::Key::V, sf::Keyboard::Key::W, sf::Keyboard::Key::X,
    sf::Keyboard::Key::Y, sf::Keyboard::Key::Z,
    sf::Keyboard::Key::Num0, sf::Keyboard::Key::Num1, sf::Keyboard::Key::Num2,
    sf::Keyboard::Key::Num3, sf::Keyboard::Key::Num4, sf::Keyboard::Key::Num5,
    sf::Keyboard::Key::Num6, sf::Keyboard::Key::Num7, sf::Keyboard::Key::Num8,
    sf::Keyboard::Key::Num9, sf::Keyboard::Key::Space,
    sf::Keyboard::Key::Escape, sf::Keyboard::Key::Enter,
    sf::Keyboard::Key::LControl, sf::Keyboard::Key::RControl,
    sf::Keyboard::Key::LAlt, sf::Keyboard::Key::RAlt,
    sf::Keyboard::Key::LShift, sf::Keyboard::Key::RShift
};

static const std::vector<sf::Mouse::Button> buttons =
{
    sf::Mouse::Button::Left, sf::Mouse::Button::Right, sf::Mouse::Button::Middle
};

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
