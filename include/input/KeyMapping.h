#pragma once

#include "input/Input.h"
#include <optional>

#ifdef USE_SFML
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

namespace ac
{

// For the SFML event pump. nullopt means the key has no engine equivalent, and it is dropped.
std::optional<Key_t> KeyFromSfKey(sf::Keyboard::Key key);
std::optional<MouseButton_t> MouseButtonFromSfButton(sf::Mouse::Button button);
ModifierState_t GetModifierState();

} // namespace ac

#endif // USE_SFML
