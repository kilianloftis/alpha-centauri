#include "ui/world/EndTurnButton.h"
#include "graphics/Graphics.h"

namespace ac
{

namespace
{

constexpr Color_t k_IdleFillColor    {40, 40, 70, 255};
constexpr Color_t k_ReadyFillColor   {40, 120, 50, 255};
constexpr Color_t k_BorderColor      {100, 100, 160, 255};
constexpr Color_t k_ReadyBorderColor {120, 220, 100, 255};
constexpr unsigned int k_FontSize    = 16;
constexpr float k_TextPadX           = 10.0f;
constexpr float k_TextPadY           = 8.0f;

} // namespace

EndTurnButton::EndTurnButton(WindowLayout_t layout, std::function<void()> onEndTurn)
    : UIElement(layout)
    , m_onEndTurn(std::move(onEndTurn))
{
}

void EndTurnButton::Render(Graphics& rGraphics)
{
    const Color_t& rFill = m_bReady ? k_ReadyFillColor : k_IdleFillColor;
    const Color_t& rBorder = m_bReady ? k_ReadyBorderColor : k_BorderColor;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, rFill);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, rBorder);
    rGraphics.DrawText(
        "End Turn",
        m_layout.x + k_TextPadX,
        m_layout.y + k_TextPadY,
        k_FontSize,
        Color_t::White());
}

void EndTurnButton::HandleMouseClick(const MouseEvent_t& /*rEvent*/)
{
    if (m_onEndTurn)
    {
        m_onEndTurn();
    }
}

} // namespace ac
