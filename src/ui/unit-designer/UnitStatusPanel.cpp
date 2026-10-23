#include "ui/unit-designer/UnitStatusPanel.h"
#include "game/units/UnitManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"
#include <sstream>

namespace ac
{

UnitStatusPanel::UnitStatusPanel(
    std::function<const UnitDesign*()> getSelectedDesign,
    const UnitManager& rUnitManager,
    WindowLayout_t layout
)
    : UIElement(layout)
    , m_getSelectedDesign(std::move(getSelectedDesign))
    , m_rUnitManager(rUnitManager)
{}

void UnitStatusPanel::Render(Graphics& rGraphics)
{
    const auto& style = Style().unitStatusPanel;
    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        style.borderColor, Style().outline.width);

    const float padding = m_layout.width * style.paddingRatio;
    const unsigned int headerSize =
        static_cast<unsigned int>(m_layout.width * style.headerFontSizeRatio);
    const unsigned int statSize =
        static_cast<unsigned int>(m_layout.width * style.statFontSizeRatio);
    const float lineH = m_layout.width * style.lineHeightRatio;
    const float textX = m_layout.x + padding;
    const float top = m_layout.y + padding;

    rGraphics.DrawText("Unit Status", textX, top, headerSize, style.headerColor);

    const UnitDesign* pDesign = m_getSelectedDesign();
    if (!pDesign)
    {
        rGraphics.DrawText(
            "No design\nselected", textX, top + lineH, statSize, style.mutedTextColor);
        return;
    }

    rGraphics.DrawText(
        pDesign->GetName(), textX, top + lineH * style.designNameLineIndex, statSize,
        style.textColor);

    std::ostringstream oss;
    oss << "Active: " << CountActive_(*pDesign);
    rGraphics.DrawText(oss.str(), textX, top + lineH * style.activeCountLineIndex, statSize,
                       style.textColor);

    // TODO: count in-production once base production integrates with UnitDesign
    rGraphics.DrawText(
        "In Prod: -", textX, top + lineH * style.inProdLineIndex, statSize, style.mutedTextColor);
}

int UnitStatusPanel::CountActive_(const UnitDesign& rDesign) const
{
    int count = 0;
    for (const Unit& rUnit : m_rUnitManager.Units())
    {
        if (&rUnit.GetDesign() == &rDesign)
        {
            ++count;
        }
    }
    return count;
}

} // namespace ac
