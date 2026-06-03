#pragma once

#include "input/Input.h"
#include <optional>

namespace ac {

void PushPendingKeyEvent(Key key);
std::optional<Key> PopPendingKeyEvent();

} // namespace ac
