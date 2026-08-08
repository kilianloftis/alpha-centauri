#include "ui/base/SupportDisplay.h"

#include "game/faction/base/BaseManager.h"
#include "game/faction/base/HomeBaseIndex.h"
#include "game/units/Unit.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"
#include "ui/world/UnitMarkerRenderer.h"


namespace ac
{

SupportDisplay::SupportDisplay(const BaseManager& rBase, WindowLayout_t layout)
    : UIElement(layout)
    , m_rBase(rBase)
{
}

void SupportDisplay::Render(Graphics& rGraphics)
{
    const auto& s = Style().supportDisplay;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.borderColor);

    const float padding = m_layout.width * s.paddingRatio;
    const float iconSize = m_layout.width * s.iconSizeRatio;
    const float gap = m_layout.width * s.iconGapRatio;
    const float left = m_layout.x + padding;
    const float right = m_layout.x + m_layout.width - padding;
    const float bottom = m_layout.y + m_layout.height - padding;
    float x = left;
    float y = m_layout.y + padding;

    for (const Unit* pUnit : m_rBase.GetHomeUnits().GetUnits())
    {
        if (!pUnit)
        {
            continue;
        }
        if (x + iconSize > right)
        {
            x = left;
            y += iconSize + gap;
        }
        if (y + iconSize > bottom)
        {
            break;
        }

        UnitMarkerRenderer::DrawMarker(
            rGraphics,
            *pUnit,
            Rectangle_t{x, y, iconSize, iconSize},
            false);
        x += iconSize + gap;
    }
}

} // namespace ac
