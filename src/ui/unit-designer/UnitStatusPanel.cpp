#include "ui/unit-designer/UnitStatusPanel.h"
#include "game/faction/UnitManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"
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
    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        Style().unitStatusPanel.backgroundColor
    );
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        Style().unitStatusPanel.borderColor
    );

    const float padding = m_layout.width * Style().unitStatusPanel.paddingRatio;
    const unsigned int headerSize = static_cast<unsigned int>(
        m_layout.width * Style().unitStatusPanel.headerFontSizeRatio);
    const unsigned int statSize = static_cast<unsigned int>(
        m_layout.width * Style().unitStatusPanel.statFontSizeRatio);
    const float lineH = m_layout.width * Style().unitStatusPanel.lineHeightRatio;

    rGraphics.DrawText(
        "Unit Status",
        m_layout.x + padding,
        m_layout.y + padding,
        headerSize,
        Style().unitStatusPanel.headerColor
    );

    const UnitDesign* pDesign = m_getSelectedDesign();
    if (!pDesign)
    {
        rGraphics.DrawText(
            "No design\nselected",
            m_layout.x + padding,
            m_layout.y + padding + lineH,
            statSize,
            Style().unitStatusPanel.mutedTextColor
        );
        return;
    }

    rGraphics.DrawText(
        pDesign->GetName(),
        m_layout.x + padding,
        m_layout.y + padding + lineH * Style().unitStatusPanel.designNameLineIndex,
        statSize
    );

    int activeCount = 0;
    if (m_pUnitManager)
    {
        for (const Unit& rUnit : m_pUnitManager->Units())
        {
            if (&rUnit.GetDesign() == pDesign)
            {
                ++activeCount;
            }
        }
    }

    std::ostringstream oss;
    oss << "Active: " << activeCount;
    rGraphics.DrawText(
        oss.str(),
        m_layout.x + padding,
        m_layout.y + padding + lineH * Style().unitStatusPanel.activeCountLineIndex,
        statSize
    );

    // TODO: count in-production once base production integrates with UnitDesign
    rGraphics.DrawText(
        "In Prod: -",
        m_layout.x + padding,
        m_layout.y + padding + lineH * Style().unitStatusPanel.inProdLineIndex,
        statSize,
        Style().unitStatusPanel.mutedTextColor
    );
}

} // namespace ac
