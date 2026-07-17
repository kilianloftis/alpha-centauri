#pragma once

#include "ui/UIElement.h"
#include "graphics/Graphics.h"
#include <functional>

namespace ac
{

class EndTurnButton : public UIElement
{
public:
    EndTurnButton(WindowLayout_t layout, std::function<void()> onEndTurn);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

    void SetReady(bool bReady) { m_bReady = bReady; }
    bool IsReady() const { return m_bReady; }

private:
    std::function<void()> m_onEndTurn;
    bool m_bReady = false;
};

} // namespace ac
