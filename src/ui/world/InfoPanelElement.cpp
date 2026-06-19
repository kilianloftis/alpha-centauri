#include "ui/world/InfoPanelElement.h"
#include "graphics/Graphics.h"

namespace ac
{

void InfoPanelElement::Render(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{20, 20, 40});
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{100, 100, 160});
    const auto& rLines = m_infoLines;
    if (!rLines.empty())
    {
        const float colWidth = m_layout.width / static_cast<float>(rLines.size());
        const float textY = m_layout.y + (m_layout.height - 20.f) * 0.5f;
        for (size_t i = 0; i < rLines.size(); ++i)
        {
            rGraphics.DrawText(rLines[i].text, m_layout.x + static_cast<float>(i) * colWidth + 10.f, textY, 18, rLines[i].color);
        }
    }
}

} // namespace ac
