#include "input/KeyMapping.h"

namespace ac
{

std::optional<Key_t>KeyFromAscii(char raw)
{
switch (raw)
{
    case 'a': case 'A': return Key_t::A;
    case 'b': case 'B': return Key_t::B;
    case 'c': case 'C': return Key_t::C;
    case 'd': case 'D': return Key_t::D;
    case 'e': case 'E': return Key_t::E;
    case 'f': case 'F': return Key_t::F;
    case 'g': case 'G': return Key_t::G;
    case 'h': case 'H': return Key_t::H;
    case 'i': case 'I': return Key_t::I;
    case 'j': case 'J': return Key_t::J;
    case 'k': case 'K': return Key_t::K;
    case 'l': case 'L': return Key_t::L;
    case 'm': case 'M': return Key_t::M;
    case 'n': case 'N': return Key_t::N;
    case 'o': case 'O': return Key_t::O;
    case 'p': case 'P': return Key_t::P;
    case 'q': case 'Q': return Key_t::Q;
    case 'r': case 'R': return Key_t::R;
    case 's': case 'S': return Key_t::S;
    case 't': case 'T': return Key_t::T;
    case 'u': case 'U': return Key_t::U;
    case 'v': case 'V': return Key_t::V;
    case 'w': case 'W': return Key_t::W;
    case 'x': case 'X': return Key_t::X;
    case 'y': case 'Y': return Key_t::Y;
    case 'z': case 'Z': return Key_t::Z;
    case '0': return Key_t::Num0;
    case '1': return Key_t::Num1;
    case '2': return Key_t::Num2;
    case '3': return Key_t::Num3;
    case '4': return Key_t::Num4;
    case '5': return Key_t::Num5;
    case '6': return Key_t::Num6;
    case '7': return Key_t::Num7;
    case '8': return Key_t::Num8;
    case '9': return Key_t::Num9;
    case ' ': return Key_t::Space;
    case '\n': return Key_t::Enter;
    default: return std::nullopt;
}
}

std::optional<char> KeyToAscii(Key_t key)
{
switch (key)
{
    case Key_t::A: return 'a';
    case Key_t::B: return 'b';
    case Key_t::C: return 'c';
    case Key_t::D: return 'd';
    case Key_t::E: return 'e';
    case Key_t::F: return 'f';
    case Key_t::G: return 'g';
    case Key_t::H: return 'h';
    case Key_t::I: return 'i';
    case Key_t::J: return 'j';
    case Key_t::K: return 'k';
    case Key_t::L: return 'l';
    case Key_t::M: return 'm';
    case Key_t::N: return 'n';
    case Key_t::O: return 'o';
    case Key_t::P: return 'p';
    case Key_t::Q: return 'q';
    case Key_t::R: return 'r';
    case Key_t::S: return 's';
    case Key_t::T: return 't';
    case Key_t::U: return 'u';
    case Key_t::V: return 'v';
    case Key_t::W: return 'w';
    case Key_t::X: return 'x';
    case Key_t::Y: return 'y';
    case Key_t::Z: return 'z';
    case Key_t::Num0: return '0';
    case Key_t::Num1: return '1';
    case Key_t::Num2: return '2';
    case Key_t::Num3: return '3';
    case Key_t::Num4: return '4';
    case Key_t::Num5: return '5';
    case Key_t::Num6: return '6';
    case Key_t::Num7: return '7';
    case Key_t::Num8: return '8';
    case Key_t::Num9: return '9';
    case Key_t::Space: return ' ';
    case Key_t::Enter: return '\n';
    default: return std::nullopt;
}
}

std::string Key_tToString(Key_t key)
{
switch (key)
{
    case Key_t::A: return "A";
    case Key_t::B: return "B";
    case Key_t::C: return "C";
    case Key_t::D: return "D";
    case Key_t::E: return "E";
    case Key_t::F: return "F";
    case Key_t::G: return "G";
    case Key_t::H: return "H";
    case Key_t::I: return "I";
    case Key_t::J: return "J";
    case Key_t::K: return "K";
    case Key_t::L: return "L";
    case Key_t::M: return "M";
    case Key_t::N: return "N";
    case Key_t::O: return "O";
    case Key_t::P: return "P";
    case Key_t::Q: return "Q";
    case Key_t::R: return "R";
    case Key_t::S: return "S";
    case Key_t::T: return "T";
    case Key_t::U: return "U";
    case Key_t::V: return "V";
    case Key_t::W: return "W";
    case Key_t::X: return "X";
    case Key_t::Y: return "Y";
    case Key_t::Z: return "Z";
    case Key_t::Num0: return "0";
    case Key_t::Num1: return "1";
    case Key_t::Num2: return "2";
    case Key_t::Num3: return "3";
    case Key_t::Num4: return "4";
    case Key_t::Num5: return "5";
    case Key_t::Num6: return "6";
    case Key_t::Num7: return "7";
    case Key_t::Num8: return "8";
    case Key_t::Num9: return "9";
    case Key_t::Space: return "Space";
    case Key_t::Escape: return "Escape";
    case Key_t::Enter: return "Enter";
    default: return "Unknown";
}
}

#ifdef USE_SFML
std::optional<Key_t>KeyFromSfKey(sf::Keyboard::Key key)
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
    case sf::Keyboard::Key::Space: return Key_t::Space;
    case sf::Keyboard::Key::Escape: return Key_t::Escape;
    case sf::Keyboard::Key::Enter: return Key_t::Enter;
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
    default: return Key_t::Unknown;
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


#endif

} // namespace ac
