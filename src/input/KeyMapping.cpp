#include "input/KeyMapping.h"

namespace ac {

std::optional<Key> KeyFromAscii(char raw) {
    switch (raw) {
        case 'a': case 'A': return Key::A;
        case 'b': case 'B': return Key::B;
        case 'c': case 'C': return Key::C;
        case 'd': case 'D': return Key::D;
        case 'e': case 'E': return Key::E;
        case 'f': case 'F': return Key::F;
        case 'g': case 'G': return Key::G;
        case 'h': case 'H': return Key::H;
        case 'i': case 'I': return Key::I;
        case 'j': case 'J': return Key::J;
        case 'k': case 'K': return Key::K;
        case 'l': case 'L': return Key::L;
        case 'm': case 'M': return Key::M;
        case 'n': case 'N': return Key::N;
        case 'o': case 'O': return Key::O;
        case 'p': case 'P': return Key::P;
        case 'q': case 'Q': return Key::Q;
        case 'r': case 'R': return Key::R;
        case 's': case 'S': return Key::S;
        case 't': case 'T': return Key::T;
        case 'u': case 'U': return Key::U;
        case 'v': case 'V': return Key::V;
        case 'w': case 'W': return Key::W;
        case 'x': case 'X': return Key::X;
        case 'y': case 'Y': return Key::Y;
        case 'z': case 'Z': return Key::Z;
        case '0': return Key::Num0;
        case '1': return Key::Num1;
        case '2': return Key::Num2;
        case '3': return Key::Num3;
        case '4': return Key::Num4;
        case '5': return Key::Num5;
        case '6': return Key::Num6;
        case '7': return Key::Num7;
        case '8': return Key::Num8;
        case '9': return Key::Num9;
        case ' ': return Key::Space;
        case '\n': return Key::Enter;
        default: return std::nullopt;
    }
}

std::optional<char> KeyToAscii(Key key) {
    switch (key) {
        case Key::A: return 'a';
        case Key::B: return 'b';
        case Key::C: return 'c';
        case Key::D: return 'd';
        case Key::E: return 'e';
        case Key::F: return 'f';
        case Key::G: return 'g';
        case Key::H: return 'h';
        case Key::I: return 'i';
        case Key::J: return 'j';
        case Key::K: return 'k';
        case Key::L: return 'l';
        case Key::M: return 'm';
        case Key::N: return 'n';
        case Key::O: return 'o';
        case Key::P: return 'p';
        case Key::Q: return 'q';
        case Key::R: return 'r';
        case Key::S: return 's';
        case Key::T: return 't';
        case Key::U: return 'u';
        case Key::V: return 'v';
        case Key::W: return 'w';
        case Key::X: return 'x';
        case Key::Y: return 'y';
        case Key::Z: return 'z';
        case Key::Num0: return '0';
        case Key::Num1: return '1';
        case Key::Num2: return '2';
        case Key::Num3: return '3';
        case Key::Num4: return '4';
        case Key::Num5: return '5';
        case Key::Num6: return '6';
        case Key::Num7: return '7';
        case Key::Num8: return '8';
        case Key::Num9: return '9';
        case Key::Space: return ' ';
        case Key::Enter: return '\n';
        default: return std::nullopt;
    }
}

std::string KeyToString(Key key) {
    switch (key) {
        case Key::A: return "A";
        case Key::B: return "B";
        case Key::C: return "C";
        case Key::D: return "D";
        case Key::E: return "E";
        case Key::F: return "F";
        case Key::G: return "G";
        case Key::H: return "H";
        case Key::I: return "I";
        case Key::J: return "J";
        case Key::K: return "K";
        case Key::L: return "L";
        case Key::M: return "M";
        case Key::N: return "N";
        case Key::O: return "O";
        case Key::P: return "P";
        case Key::Q: return "Q";
        case Key::R: return "R";
        case Key::S: return "S";
        case Key::T: return "T";
        case Key::U: return "U";
        case Key::V: return "V";
        case Key::W: return "W";
        case Key::X: return "X";
        case Key::Y: return "Y";
        case Key::Z: return "Z";
        case Key::Num0: return "0";
        case Key::Num1: return "1";
        case Key::Num2: return "2";
        case Key::Num3: return "3";
        case Key::Num4: return "4";
        case Key::Num5: return "5";
        case Key::Num6: return "6";
        case Key::Num7: return "7";
        case Key::Num8: return "8";
        case Key::Num9: return "9";
        case Key::Space: return "Space";
        case Key::Escape: return "Escape";
        case Key::Enter: return "Enter";
        default: return "Unknown";
    }
}

#ifdef USE_SFML
std::optional<Key> KeyFromSfKey(sf::Keyboard::Key key) {
    switch (key) {
        case sf::Keyboard::Key::A: return Key::A;
        case sf::Keyboard::Key::B: return Key::B;
        case sf::Keyboard::Key::C: return Key::C;
        case sf::Keyboard::Key::D: return Key::D;
        case sf::Keyboard::Key::E: return Key::E;
        case sf::Keyboard::Key::F: return Key::F;
        case sf::Keyboard::Key::G: return Key::G;
        case sf::Keyboard::Key::H: return Key::H;
        case sf::Keyboard::Key::I: return Key::I;
        case sf::Keyboard::Key::J: return Key::J;
        case sf::Keyboard::Key::K: return Key::K;
        case sf::Keyboard::Key::L: return Key::L;
        case sf::Keyboard::Key::M: return Key::M;
        case sf::Keyboard::Key::N: return Key::N;
        case sf::Keyboard::Key::O: return Key::O;
        case sf::Keyboard::Key::P: return Key::P;
        case sf::Keyboard::Key::Q: return Key::Q;
        case sf::Keyboard::Key::R: return Key::R;
        case sf::Keyboard::Key::S: return Key::S;
        case sf::Keyboard::Key::T: return Key::T;
        case sf::Keyboard::Key::U: return Key::U;
        case sf::Keyboard::Key::V: return Key::V;
        case sf::Keyboard::Key::W: return Key::W;
        case sf::Keyboard::Key::X: return Key::X;
        case sf::Keyboard::Key::Y: return Key::Y;
        case sf::Keyboard::Key::Z: return Key::Z;
        case sf::Keyboard::Key::Num0: return Key::Num0;
        case sf::Keyboard::Key::Num1: return Key::Num1;
        case sf::Keyboard::Key::Num2: return Key::Num2;
        case sf::Keyboard::Key::Num3: return Key::Num3;
        case sf::Keyboard::Key::Num4: return Key::Num4;
        case sf::Keyboard::Key::Num5: return Key::Num5;
        case sf::Keyboard::Key::Num6: return Key::Num6;
        case sf::Keyboard::Key::Num7: return Key::Num7;
        case sf::Keyboard::Key::Num8: return Key::Num8;
        case sf::Keyboard::Key::Num9: return Key::Num9;
        case sf::Keyboard::Key::Space: return Key::Space;
        case sf::Keyboard::Key::Escape: return Key::Escape;
        case sf::Keyboard::Key::Enter: return Key::Enter;
        default: return Key::Unknown;
    }
}
#endif

} // namespace ac
