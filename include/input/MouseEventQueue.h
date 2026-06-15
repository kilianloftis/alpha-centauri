#pragma once

#include "input/Input.h"
#include <optional>

namespace ac
{

void PushPendingMouseEvent_t(MouseEvent_t event);
std::optional<MouseEvent_t> PopPendingMouseEvent();

} // namespace ac
