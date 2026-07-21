#include "ui/world/EndTurnButton.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

EndTurnButton::EndTurnButton(WindowLayout_t layout, std::function<void()> onEndTurn)
    : UIElement(layout)
    , m_onEndTurn(std::move(onEndTurn))
{
}

void EndTurnButton::Render(Graphics& rGraphics)
{
    const auto& s = Style().endTurnButton;
    const Color_t& rFill = m_bReady ? s.readyFillColor : s.idleFillColor;
    const Color_t& rBorder = m_bReady ? s.readyBorderColor : s.borderColor;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, rFill);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, rBorder);
    rGraphics.DrawText(
        "End Turn",
        m_layout.x + s.textPadX,
        m_layout.y + s.textPadY,
        s.fontSize,
        s.labelColor);
}

void EndTurnButton::HandleMouseClick(const MouseEvent_t& /*rEvent*/)
{
    if (m_onEndTurn)
    {
        m_onEndTurn();
    }
}

} // namespace ac
