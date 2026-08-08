#include "ui/commlinks/CouncilButton.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

CouncilButton::CouncilButton(WindowLayout_t layout, std::function<void()> onOpenCouncil)
    : UIElement(layout)
    , m_onOpenCouncil(std::move(onOpenCouncil))
{
}

void CouncilButton::Render(Graphics& rGraphics)
{
    const auto& s = Style().commlinksButton;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.fillColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.borderColor);
    rGraphics.DrawText(
        "Council",
        m_layout.x + s.textPadX,
        m_layout.y + s.textPadY,
        s.fontSize,
        s.labelColor);
}

void CouncilButton::HandleMouseClick(const MouseEvent_t& rEvent)
{
    // Every peer control requires the left button; this one opened the council on any of them.
    if (rEvent.button != MouseButton_t::Left)
    {
        return;
    }
    if (m_onOpenCouncil)
    {
        m_onOpenCouncil();
    }
}

} // namespace ac
