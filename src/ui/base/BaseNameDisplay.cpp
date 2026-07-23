#include "ui/base/BaseNameDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"
#include <stdexcept>

namespace ac
{

BaseNameDisplay::BaseNameDisplay(const BaseManager* pBase, WindowLayout_t layout)
    : UIElement(layout)
    , m_pBase(pBase)
{
}

void BaseNameDisplay::Render(Graphics& rGraphics)
{
    if (!m_pBase)
    {
        throw std::runtime_error("BaseNameDisplay: No base manager set");
    }

    const auto& s = Style().infoPanel;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.borderColor);
    rGraphics.DrawText(
        m_pBase->GetName(),
        m_layout.x + s.textHorizontalPadding,
        m_layout.y + (m_layout.height - s.textHeightEstimate) * s.textVerticalCenterRatio,
        s.fontSize,
        s.defaultLineColor);
}

} // namespace ac
