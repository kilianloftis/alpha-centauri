#include "ui/world/InfoPanelElement.h"
#include "graphics/Graphics.h"

namespace ac
{

void InfoPanelElement::Draw(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(m_x, m_y, m_width, m_height, Color{20, 20, 40});
    rGraphics.DrawRect(m_x, m_y, m_width, m_height, Color{100, 100, 160});
    const auto& rLines = m_infoLines;
    if (!rLines.empty())
    {
        const float colWidth = m_width / static_cast<float>(rLines.size());
        const float textY = m_y + (m_height - 20.f) * 0.5f;
        for (size_t i = 0; i < rLines.size(); ++i)
        {
            rGraphics.DrawText(rLines[i].text, m_x + static_cast<float>(i) * colWidth + 10.f, textY, 18, rLines[i].color);
        }
    }
    else
    {
        rGraphics.DrawText(m_title, m_x + 10.f, m_y + 5.f, 16);
    }
}

void InfoPanelElement::Update(float /*deltaTime*/)
{
}

} // namespace ac
