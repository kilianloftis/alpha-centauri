#include "ui/base/GrowthDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "graphics/Graphics.h"
#include <sstream>
#include <stdexcept>

namespace ac
{

namespace
{

constexpr Color k_BackgroundColor      {20, 20, 20, 255};
constexpr float k_HeaderFontSizeRatio  = 0.04f;
constexpr float k_EntryFontSizeRatio   = 0.03f;
constexpr float k_LineHeightRatio      = 0.05f;
constexpr float k_LeftPaddingRatio     = 0.02f;
constexpr float k_StockpileLineIndex     = 1.0f;
constexpr float k_RequiredLineIndex      = 2.0f;
constexpr float k_ProductionLineIndex    = 3.0f;

} // namespace

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

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        k_BackgroundColor
    );

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * k_HeaderFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(m_layout.height * k_EntryFontSizeRatio);
    const float lineHeight = m_layout.height * k_LineHeightRatio;
    const float leftPadding = m_layout.width * k_LeftPaddingRatio;

    rGraphics.DrawText("Growth", m_layout.x + leftPadding, m_layout.y, headerFontSize);

    std::ostringstream oss;
    oss << "Stockpile: " << m_pBase->GetPopulation().GetNutrientStockpile();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * k_StockpileLineIndex, entryFontSize);

    oss.str("");
    oss << "Required: " << m_pBase->GetNutrientsRequired();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * k_RequiredLineIndex, entryFontSize);

    oss.str("");
    oss << "Production: " << m_pBase->GetNutrientProduction();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * k_ProductionLineIndex, entryFontSize);
}

} // namespace ac
