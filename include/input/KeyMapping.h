#pragma once

#include "input/Input.h"
#include <optional>

#ifdef USE_SFML
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

namespace ac
{

// SFML -> engine translation, used only by the SFML windowing backend's event pump.
// nullopt means "this input has no engine equivalent"; the caller drops the event rather than
// forwarding an Unknown one.
std::optional<Key_t> KeyFromSfKey(sf::Keyboard::Key key);
std::optional<MouseButton_t> MouseButtonFromSfButton(sf::Mouse::Button button);
ModifierState_t GetModifierState();

} // namespace ac

#endif // USE_SFML
