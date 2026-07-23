#include "ui/PlaceholderPanel.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

PlaceholderPanel::PlaceholderPanel(std::string label, WindowLayout_t layout)
    : UIElement(layout)
    , m_label(std::move(label))
{
}

void PlaceholderPanel::Render(Graphics& rGraphics)
{
    const auto& s = Style().infoPanel;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.borderColor);
    rGraphics.DrawText(
        m_label,
        m_layout.x + s.textHorizontalPadding,
        m_layout.y + (m_layout.height - s.textHeightEstimate) * s.textVerticalCenterRatio,
        s.fontSize,
        s.defaultLineColor);
}

} // namespace ac
