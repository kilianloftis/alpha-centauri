#include "ui/world/InfoPanelElement.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

void InfoPanelElement::Render(Graphics& rGraphics)
{
    const auto& s = Style().infoPanel;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.borderColor);
    const auto& rLines = m_infoLines;
    if (!rLines.empty())
    {
        const float colWidth = m_layout.width / static_cast<float>(rLines.size());
        const float textY = m_layout.y + (m_layout.height - s.textHeightEstimate) * s.textVerticalCenterRatio;
        for (size_t i = 0; i < rLines.size(); ++i)
        {
            rGraphics.DrawText(
                rLines[i].text,
                m_layout.x + static_cast<float>(i) * colWidth + s.textHorizontalPadding,
                textY,
                s.fontSize,
                rLines[i].color
            );
        }
    }
}

} // namespace ac
