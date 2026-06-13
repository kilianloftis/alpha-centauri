#pragma once

#include "input/Input.h"
#include <optional>
#include <string>

#ifdef USE_SFML
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#endif

namespace ac
{

std::optional<Key_t>KeyFromAscii(char raw);
std::optional<char> KeyToAscii(Key_t key);
std::string Key_tToString(Key_t key);
#ifdef USE_SFML
std::optional<Key_t>KeyFromSfKey(sf::Keyboard::Key key);
std::optional<MouseButton_t> MouseButtonFromSfButton(sf::Mouse::Button button);
ModifierState_t GetModifierState();
#endif

} // namespace ac
