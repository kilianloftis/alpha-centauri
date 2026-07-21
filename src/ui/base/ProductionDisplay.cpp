#include "ui/base/ProductionDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"
#include <functional>
#include <sstream>
#include <stdexcept>

namespace ac
{

ProductionDisplay::ProductionDisplay(
    const BaseManager* pBase,
    WindowLayout_t layout,
    std::function<void()> onClicked
)
    : UIElement(layout)
    , m_onClicked(std::move(onClicked))
    , m_pBase(pBase)
{}

void ProductionDisplay::Render(Graphics& rGraphics)
{
    if (!m_pBase)
    {
        throw std::runtime_error("ProductionDisplay: No base manager set");
    }

    const auto& style = Style().productionDisplay;

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        style.backgroundColor
    );

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(m_layout.height * style.entryFontSizeRatio);
    const float lineHeight   = m_layout.height * style.lineHeightRatio;
    const float leftPadding  = m_layout.width  * style.leftPaddingRatio;

    const ProductionManager& rProduction = m_pBase->GetProduction();
    const IConstructable* pCurrentProduction = rProduction.GetCurrentProduction();
    const std::string header = pCurrentProduction ? "Production: " + pCurrentProduction->GetName() : "Production: (none)";
    rGraphics.DrawText(header, m_layout.x + leftPadding, m_layout.y, headerFontSize, style.textColor);

    std::ostringstream oss;

    oss << "Stockpile: " << rProduction.GetMineralStockpile();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * style.stockpileLineIndex, entryFontSize, style.textColor);

    oss.str("");
    oss << "Required: ";
    if (pCurrentProduction)
    {
        oss << m_pBase->GetMineralCost();
    }
    else
    {
        oss << "-";
    }
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * style.requiredLineIndex, entryFontSize, style.textColor);

    oss.str("");
    oss << "Minerals/turn: " << m_pBase->GetMineralProduction();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * style.productionLineIndex, entryFontSize, style.textColor);
}

void ProductionDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button == MouseButton_t::Left && m_onClicked)
    {
        m_onClicked();
    }
}

} // namespace ac
