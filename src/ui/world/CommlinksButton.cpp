#include "ui/world/CommlinksButton.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

CommlinksButton::CommlinksButton(WindowLayout_t layout, std::function<void()> onOpenCommlinks)
    : UIElement(layout)
    , m_onOpenCommlinks(std::move(onOpenCommlinks))
{
}

void CommlinksButton::Render(Graphics& rGraphics)
{
    const auto& s = Style().commlinksButton;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.fillColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.borderColor);
    rGraphics.DrawText(
        "Commlinks",
        m_layout.x + s.textPadX,
        m_layout.y + s.textPadY,
        s.fontSize,
        s.labelColor);
}

void CommlinksButton::HandleMouseClick(const MouseEvent_t& /*rEvent*/)
{
    if (m_onOpenCommlinks)
    {
        m_onOpenCommlinks();
    }
}

} // namespace ac
