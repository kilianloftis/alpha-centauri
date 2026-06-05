#pragma once

#include "input/Input.h"
#include <optional>
#include <string>

#ifdef USE_SFML
#include <SFML/Window/Keyboard.hpp>
#endif

namespace ac
{

std::optional<Key> KeyFromAscii(char raw);
std::optional<char> KeyToAscii(Key key);
std::string KeyToString(Key key);
#ifdef USE_SFML
std::optional<Key> KeyFromSfKey(sf::Keyboard::Key key);
#endif

} // namespace ac
