#pragma once

#include "input/Input.h"
#include <optional>

namespace ac
{

void PushPendingKeyEvent_t(const KeyEvent_t& rEvent);
std::optional<KeyEvent_t> PopPendingKeyEvent();

} // namespace ac
