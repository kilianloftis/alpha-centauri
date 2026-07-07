#pragma once

#include "input/Input.h"
#include <optional>

namespace ac
{

struct LastMousePosition_t
{
    int x;
    int y;
};

void PushPendingMouseEvent_t(MouseEvent_t event);
std::optional<MouseEvent_t> PopPendingMouseEvent();
bool HasLastMousePosition();
LastMousePosition_t GetLastMousePosition();

} // namespace ac
