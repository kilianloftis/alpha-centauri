#include "ui/base/GrowthDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"
#include <sstream>
#include <stdexcept>

namespace ac
{

GrowthDisplay::GrowthDisplay(
    const BaseManager* pBase,
    WindowLayout_t layout
)
    : UIElement(layout)
    , m_pBase(pBase)
{}

void GrowthDisplay::Render(Graphics& rGraphics)
{
    if (!m_pBase)
    {
        throw std::runtime_error("GrowthDisplay: No base manager set");
    }

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
    oss << "Stockpile: " << m_pBase->GetPopulation().GetNutrientStockpile();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * style.stockpileLineIndex, entryFontSize, style.textColor);

    oss.str("");
    oss << "Required: " << m_pBase->GetNutrientsRequired();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * style.requiredLineIndex, entryFontSize, style.textColor);

    oss.str("");
    oss << "Production: " << m_pBase->GetNutrientProduction();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * style.productionLineIndex, entryFontSize, style.textColor);
}

} // namespace ac
