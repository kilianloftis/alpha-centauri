#pragma once

#include "input/Input.h"
#include <optional>

namespace ac
{

void PushPendingKeyEvent_t(Key_t key);
std::optional<Key_t>PopPendingKeyEvent();

} // namespace ac
