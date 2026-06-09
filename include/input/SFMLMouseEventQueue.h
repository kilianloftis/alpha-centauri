#pragma once

#include "input/Input.h"
#include <optional>

namespace ac
{

void PushPendingMouseEvent(MouseEvent event);
std::optional<MouseEvent> PopPendingMouseEvent();

} // namespace ac
