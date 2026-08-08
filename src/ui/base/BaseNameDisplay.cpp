#include "ui/base/BaseNameDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

BaseNameDisplay::BaseNameDisplay(const BaseManager& rBase, WindowLayout_t layout)
    : UIElement(layout)
    , m_rBase(rBase)
{
}

void BaseNameDisplay::Render(Graphics& rGraphics)
{
    const auto& s = Style().infoPanel;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.borderColor);
    rGraphics.DrawText(
        m_rBase.GetName(),
        m_layout.x + s.textHorizontalPadding,
        m_layout.y + (m_layout.height - s.textHeightEstimate) * s.textVerticalCenterRatio,
        s.fontSize,
        s.defaultLineColor);
}

} // namespace ac
