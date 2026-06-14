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
    virtual ~IGameView() = default;

    virtual void OnPushed() {}
    virtual void OnPopped() {}

    virtual void Render(Graphics& rGraphics) = 0;
    virtual void Update(float deltaTime) = 0;

    virtual bool HandleKey(const KeyEvent_t& rEvent) = 0;
    virtual bool HandleMouse(const MouseEvent_t& rEvent) = 0;

    virtual std::vector<UIElement*> GetElements() = 0;
};

} // namespace ac
