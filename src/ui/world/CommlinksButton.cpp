#include "ui/world/CommlinksButton.h"
#include "graphics/Graphics.h"

namespace ac
{

namespace
{

constexpr Color_t k_FillColor   {40, 40, 70, 255};
constexpr Color_t k_BorderColor {100, 100, 160, 255};
constexpr unsigned int k_FontSize = 16;
constexpr float k_TextPadX        = 10.0f;
constexpr float k_TextPadY        = 8.0f;

} // namespace

CommlinksButton::CommlinksButton(WindowLayout_t layout, std::function<void()> onOpenCommlinks)
    : UIElement(layout)
    , m_onOpenCommlinks(std::move(onOpenCommlinks))
{
}

void CommlinksButton::Render(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_FillColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BorderColor);
    rGraphics.DrawText(
        "Commlinks",
        m_layout.x + k_TextPadX,
        m_layout.y + k_TextPadY,
        k_FontSize,
        Color_t::White());
}

void CommlinksButton::HandleMouseClick(const MouseEvent_t& /*rEvent*/)
{
    if (m_onOpenCommlinks)
    {
        m_onOpenCommlinks();
    }
}

} // namespace ac
