#pragma once

#include "ui/UIElement.h"
#include "graphics/Graphics.h"
#include <functional>

namespace ac
{

class CouncilButton : public UIElement
{
public:
    CouncilButton(WindowLayout_t layout, std::function<void()> onOpenCouncil);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    std::function<void()> m_onOpenCouncil;
};

} // namespace ac
