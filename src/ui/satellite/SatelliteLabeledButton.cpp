#include "ui/satellite/SatelliteLabeledButton.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

SatelliteLabeledButton::SatelliteLabeledButton(WindowLayout_t layout,
                                               std::string label,
                                               std::function<void()> onClick,
                                               bool bSelected)
    : UIElement(layout)
    , m_label(std::move(label))
    , m_onClick(std::move(onClick))
    , m_bSelected(bSelected)
{
}

void SatelliteLabeledButton::SetSelected(bool bSelected)
{
    m_bSelected = bSelected;
}

void SatelliteLabeledButton::Render(Graphics& rGraphics)
{
    const auto& s = Style().satelliteView;
    const Color_t& rFill = m_bSelected ? s.tabSelectedFillColor : s.tabFillColor;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, rFill);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.tabBorderColor);
    rGraphics.DrawText(
        m_label,
        m_layout.x + s.tabTextPadX,
        m_layout.y + s.tabTextPadY,
        s.tabFontSize,
        s.tabLabelColor);
}

void SatelliteLabeledButton::HandleMouseClick(const MouseEvent_t& /*rEvent*/)
{
    if (m_onClick)
    {
        m_onClick();
    }
}

} // namespace ac
