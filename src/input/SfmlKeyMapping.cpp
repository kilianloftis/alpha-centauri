#include "input/KeyMapping.h"

namespace ac
{

std::optional<Key_t> KeyFromSfKey(sf::Keyboard::Key key)
{
    switch (key)
    {
    case sf::Keyboard::Key::A: return Key_t::A;
    case sf::Keyboard::Key::B: return Key_t::B;
    case sf::Keyboard::Key::C: return Key_t::C;
    case sf::Keyboard::Key::D: return Key_t::D;
    case sf::Keyboard::Key::E: return Key_t::E;
    case sf::Keyboard::Key::F: return Key_t::F;
    case sf::Keyboard::Key::G: return Key_t::G;
    case sf::Keyboard::Key::H: return Key_t::H;
    case sf::Keyboard::Key::I: return Key_t::I;
    case sf::Keyboard::Key::J: return Key_t::J;
    case sf::Keyboard::Key::K: return Key_t::K;
    case sf::Keyboard::Key::L: return Key_t::L;
    case sf::Keyboard::Key::M: return Key_t::M;
    case sf::Keyboard::Key::N: return Key_t::N;
    case sf::Keyboard::Key::O: return Key_t::O;
    case sf::Keyboard::Key::P: return Key_t::P;
    case sf::Keyboard::Key::Q: return Key_t::Q;
    case sf::Keyboard::Key::R: return Key_t::R;
    case sf::Keyboard::Key::S: return Key_t::S;
    case sf::Keyboard::Key::T: return Key_t::T;
    case sf::Keyboard::Key::U: return Key_t::U;
    case sf::Keyboard::Key::V: return Key_t::V;
    case sf::Keyboard::Key::W: return Key_t::W;
    case sf::Keyboard::Key::X: return Key_t::X;
    case sf::Keyboard::Key::Y: return Key_t::Y;
    case sf::Keyboard::Key::Z: return Key_t::Z;
    case sf::Keyboard::Key::Num0: return Key_t::Num0;
    case sf::Keyboard::Key::Num1: return Key_t::Num1;
    case sf::Keyboard::Key::Num2: return Key_t::Num2;
    case sf::Keyboard::Key::Num3: return Key_t::Num3;
    case sf::Keyboard::Key::Num4: return Key_t::Num4;
    case sf::Keyboard::Key::Num5: return Key_t::Num5;
    case sf::Keyboard::Key::Num6: return Key_t::Num6;
    case sf::Keyboard::Key::Num7: return Key_t::Num7;
    case sf::Keyboard::Key::Num8: return Key_t::Num8;
    case sf::Keyboard::Key::Num9: return Key_t::Num9;
    case sf::Keyboard::Key::Numpad0: return Key_t::Num0;
    case sf::Keyboard::Key::Numpad1: return Key_t::Num1;
    case sf::Keyboard::Key::Numpad2: return Key_t::Num2;
    case sf::Keyboard::Key::Numpad3: return Key_t::Num3;
    case sf::Keyboard::Key::Numpad4: return Key_t::Num4;
    case sf::Keyboard::Key::Numpad5: return Key_t::Num5;
    case sf::Keyboard::Key::Numpad6: return Key_t::Num6;
    case sf::Keyboard::Key::Numpad7: return Key_t::Num7;
    case sf::Keyboard::Key::Numpad8: return Key_t::Num8;
    case sf::Keyboard::Key::Numpad9: return Key_t::Num9;
    case sf::Keyboard::Key::Space: return Key_t::Space;
    case sf::Keyboard::Key::Escape: return Key_t::Escape;
    case sf::Keyboard::Key::Enter: return Key_t::Enter;
    case sf::Keyboard::Key::Backspace: return Key_t::Backspace;
    case sf::Keyboard::Key::F1: return Key_t::F1;
    case sf::Keyboard::Key::F2: return Key_t::F2;
    case sf::Keyboard::Key::F3: return Key_t::F3;
    case sf::Keyboard::Key::F4: return Key_t::F4;
    case sf::Keyboard::Key::F5: return Key_t::F5;
    case sf::Keyboard::Key::F6: return Key_t::F6;
    case sf::Keyboard::Key::F7: return Key_t::F7;
    case sf::Keyboard::Key::F8: return Key_t::F8;
    case sf::Keyboard::Key::F9: return Key_t::F9;
    case sf::Keyboard::Key::F10: return Key_t::F10;
    case sf::Keyboard::Key::F11: return Key_t::F11;
    case sf::Keyboard::Key::F12: return Key_t::F12;
    case sf::Keyboard::Key::Up:    return Key_t::ArrowUp;
    case sf::Keyboard::Key::Down:  return Key_t::ArrowDown;
    case sf::Keyboard::Key::Left:  return Key_t::ArrowLeft;
    case sf::Keyboard::Key::Right: return Key_t::ArrowRight;
    default: return std::nullopt;
    }
}

std::optional<MouseButton_t> MouseButtonFromSfButton(sf::Mouse::Button button)
{
    switch (button)
    {
        case sf::Mouse::Button::Left: return MouseButton_t::Left;
        case sf::Mouse::Button::Right: return MouseButton_t::Right;
        case sf::Mouse::Button::Middle: return MouseButton_t::Middle;
        default: return std::nullopt;
    }
}

ModifierState_t GetModifierState()
{
    return {
        .bCtrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl),
        .bAlt = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LAlt) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RAlt),
        .bShift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift)
    };
}

} // namespace ac
