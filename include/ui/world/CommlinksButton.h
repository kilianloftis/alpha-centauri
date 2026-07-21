#pragma once

#include "ui/UIElement.h"
#include "graphics/Graphics.h"
#include <functional>

namespace ac
{

class CommlinksButton : public UIElement
{
public:
    CommlinksButton(WindowLayout_t layout, std::function<void()> onOpenCommlinks);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    std::function<void()> m_onOpenCommlinks;
};

} // namespace ac
