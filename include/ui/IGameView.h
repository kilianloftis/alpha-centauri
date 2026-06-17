#pragma once

#include "input/Input.h"
#include <vector>

namespace ac
{

class Graphics;
class UIElement;

class IGameView
{
public:
GameView(std::function<void()> onClose);
virtual ~IGameView() = default;

virtual void OnPushed() {}
virtual void OnPopped() {}

virtual void Render(Graphics& rGraphics) = 0;
virtual void Update() = 0;

virtual void HandleKey(const KeyEvent_t& rEvent) = 0;
virtual void HandleMouse(const MouseEvent_t& rEvent) = 0;

virtual void OnClose() {
    m_onClose();
}

private:
    std::function<void()> m_onClose;
}
};

} // namespace ac
