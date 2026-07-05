#include "ui/world/InfoPanelElement.h"
#include "graphics/Graphics.h"

namespace ac
{

namespace
{

constexpr Color k_BackgroundColor           {20, 20, 40, 255};
constexpr Color k_BorderColor               {100, 100, 160, 255};
constexpr float k_TextHeightEstimate        = 20.0f;
constexpr float k_TextVerticalCenterRatio     = 0.5f;
constexpr float k_TextHorizontalPadding       = 10.0f;
constexpr unsigned int k_FontSize             = 18;

} // namespace

void InfoPanelElement::Render(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BackgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BorderColor);
    const auto& rLines = m_infoLines;
    if (!rLines.empty())
    {
        const float colWidth = m_layout.width / static_cast<float>(rLines.size());
        const float textY = m_layout.y + (m_layout.height - k_TextHeightEstimate) * k_TextVerticalCenterRatio;
        for (size_t i = 0; i < rLines.size(); ++i)
        {
            rGraphics.DrawText(
                rLines[i].text,
                m_layout.x + static_cast<float>(i) * colWidth + k_TextHorizontalPadding,
                textY,
                k_FontSize,
                rLines[i].color
            );
        }
    }
}

} // namespace ac
