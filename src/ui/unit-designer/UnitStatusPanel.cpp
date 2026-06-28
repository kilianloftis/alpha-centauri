#include "ui/unit-designer/UnitStatusPanel.h"
#include "game/faction/UnitManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "graphics/Graphics.h"
#include <sstream>

namespace ac
{

UnitStatusPanel::UnitStatusPanel(
    std::function<const UnitDesign*()> getSelectedDesign,
    const UnitManager* pUnitManager,
    WindowLayout_t layout
)
    : UIElement(layout)
    , m_getSelectedDesign(std::move(getSelectedDesign))
    , m_pUnitManager(pUnitManager)
{}

void UnitStatusPanel::Render(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{20, 20, 20, 255});
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{80, 80, 80, 255});

    const float padding          = m_layout.width * k_PaddingRatio;
    const unsigned int headerSize = static_cast<unsigned int>(m_layout.width * k_HeaderFontSizeRatio);
    const unsigned int statSize   = static_cast<unsigned int>(m_layout.width * k_StatFontSizeRatio);
    const float lineH            = m_layout.width * k_LineHeightRatio;

    rGraphics.DrawText("Unit Status", m_layout.x + padding, m_layout.y + padding, headerSize, Color::Yellow());

    const UnitDesign* pDesign = m_getSelectedDesign();
    if (!pDesign)
    {
        rGraphics.DrawText(
            "No design\nselected",
            m_layout.x + padding,
            m_layout.y + padding + lineH,
            statSize,
            Color{100, 100, 100, 255}
        );
        return;
    }

    rGraphics.DrawText(pDesign->GetName(), m_layout.x + padding, m_layout.y + padding + lineH, statSize);

    int activeCount = 0;
    if (m_pUnitManager)
    {
        for (const auto& pUnit : m_pUnitManager->GetUnits())
        {
            if (&pUnit->GetDesign() == pDesign)
            {
                ++activeCount;
            }
        }
    }

    std::ostringstream oss;
    oss << "Active: " << activeCount;
    rGraphics.DrawText(oss.str(), m_layout.x + padding, m_layout.y + padding + lineH * 2.0f, statSize);

    // TODO: count in-production once base production integrates with UnitDesign
    rGraphics.DrawText(
        "In Prod: -",
        m_layout.x + padding,
        m_layout.y + padding + lineH * 3.0f,
        statSize,
        Color{100, 100, 100, 255}
    );
}

} // namespace ac
