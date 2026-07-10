#include "ui/unit-designer/UnitStatusPanel.h"
#include "game/faction/UnitManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "graphics/Graphics.h"
#include <sstream>

namespace ac
{

namespace
{

constexpr Color_t k_BackgroundColor      {20, 20, 20, 255};
constexpr Color_t k_BorderColor          {80, 80, 80, 255};
constexpr Color_t k_MutedTextColor       {100, 100, 100, 255};
constexpr float k_HeaderFontSizeRatio  = 0.08f;
constexpr float k_StatFontSizeRatio    = 0.07f;
constexpr float k_LineHeightRatio      = 0.10f;
constexpr float k_PaddingRatio         = 0.04f;
constexpr float k_DesignNameLineIndex  = 1.0f;
constexpr float k_ActiveCountLineIndex = 2.0f;
constexpr float k_InProdLineIndex      = 3.0f;

} // namespace

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
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BackgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BorderColor);

    const float padding          = m_layout.width * k_PaddingRatio;
    const unsigned int headerSize = static_cast<unsigned int>(m_layout.width * k_HeaderFontSizeRatio);
    const unsigned int statSize   = static_cast<unsigned int>(m_layout.width * k_StatFontSizeRatio);
    const float lineH            = m_layout.width * k_LineHeightRatio;

    rGraphics.DrawText("Unit Status", m_layout.x + padding, m_layout.y + padding, headerSize, Color_t::Yellow());

    const UnitDesign* pDesign = m_getSelectedDesign();
    if (!pDesign)
    {
        rGraphics.DrawText(
            "No design\nselected",
            m_layout.x + padding,
            m_layout.y + padding + lineH,
            statSize,
            k_MutedTextColor
        );
        return;
    }

    rGraphics.DrawText(pDesign->GetName(), m_layout.x + padding, m_layout.y + padding + lineH * k_DesignNameLineIndex, statSize);

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
    rGraphics.DrawText(oss.str(), m_layout.x + padding, m_layout.y + padding + lineH * k_ActiveCountLineIndex, statSize);

    // TODO: count in-production once base production integrates with UnitDesign
    rGraphics.DrawText(
        "In Prod: -",
        m_layout.x + padding,
        m_layout.y + padding + lineH * k_InProdLineIndex,
        statSize,
        k_MutedTextColor
    );
}

} // namespace ac
