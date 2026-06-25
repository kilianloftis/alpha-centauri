#include "ui/base/GrowthDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "graphics/Graphics.h"
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

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        Color{20, 20, 20, 255}
    );

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * k_HeaderFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(m_layout.height * k_EntryFontSizeRatio);
    const float lineHeight = m_layout.height * k_LineHeightRatio;
    const float leftPadding = m_layout.width * k_LeftPaddingRatio;

    rGraphics.DrawText("Growth", m_layout.x + leftPadding, m_layout.y, headerFontSize);

    std::ostringstream oss;
    oss << "Stockpile: " << m_pBase->GetNutrientStockpile();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight, entryFontSize);

    oss.str("");
    oss << "Required: " << m_pBase->GetNutrientsRequired();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * 2.0f, entryFontSize);

    oss.str("");
    oss << "Production: " << m_pBase->GetNutrientProduction();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * 3.0f, entryFontSize);
}

} // namespace ac
