#include "ui/base/GrowthDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "graphics/Graphics.h"
#include "ui/base/BaseDisplaySnapshot.h"
#include "ui/style/UiStyle.h"
#include <sstream>

namespace ac
{

GrowthDisplay::GrowthDisplay(
    const BaseManager& rBase,
    const BaseDisplaySnapshot_t& rSnapshot,
    WindowLayout_t layout
)
    : UIElement(layout)
    , m_rBase(rBase)
    , m_rSnapshot(rSnapshot)
{}

void GrowthDisplay::Render(Graphics& rGraphics)
{
    const auto& style = Style().growthDisplay;

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        style.backgroundColor
    );

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(m_layout.height * style.entryFontSizeRatio);
    const float lineHeight = m_layout.height * style.lineHeightRatio;
    const float leftPadding = m_layout.width * style.leftPaddingRatio;

    rGraphics.DrawText("Growth", m_layout.x + leftPadding, m_layout.y, headerFontSize, style.textColor);

    std::ostringstream oss;
    oss << "Stockpile: " << m_rBase.GetPopulation().GetNutrientStockpile();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * style.stockpileLineIndex, entryFontSize, style.textColor);

    oss.str("");
    oss << "Required: " << m_rSnapshot.nutrientsRequired;
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * style.requiredLineIndex, entryFontSize, style.textColor);

    oss.str("");
    oss << "Production: " << m_rSnapshot.nutrientProduction;
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * style.productionLineIndex, entryFontSize, style.textColor);
}

} // namespace ac
