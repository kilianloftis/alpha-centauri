#include "ui/base/ProductionDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "graphics/Graphics.h"
#include "ui/base/BaseDisplaySnapshot.h"
#include "ui/style/UiStyle.h"
#include <functional>
#include <sstream>

namespace ac
{

ProductionDisplay::ProductionDisplay(
    const BaseManager& rBase,
    const BaseDisplaySnapshot_t& rSnapshot,
    WindowLayout_t layout,
    std::function<void()> onClicked
)
    : UIElement(layout)
    , m_onClicked(std::move(onClicked))
    , m_rBase(rBase)
    , m_rSnapshot(rSnapshot)
{}

void ProductionDisplay::Render(Graphics& rGraphics)
{
    const auto& style = Style().productionDisplay;

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        style.backgroundColor
    );

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(m_layout.height * style.entryFontSizeRatio);
    const float lineHeight   = m_layout.height * style.lineHeightRatio;
    const float leftPadding  = m_layout.width  * style.leftPaddingRatio;

    const std::string header = m_rSnapshot.bHasProduction
                                   ? "Production: " + m_rSnapshot.productionName
                                   : "Production: (none)";
    rGraphics.DrawText(header, m_layout.x + leftPadding, m_layout.y, headerFontSize, style.textColor);

    std::ostringstream oss;

    oss << "Stockpile: " << m_rBase.GetProduction().GetMineralStockpile();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * style.stockpileLineIndex, entryFontSize, style.textColor);

    oss.str("");
    oss << "Required: ";
    if (m_rSnapshot.bHasProduction)
    {
        oss << m_rSnapshot.mineralCost;
    }
    else
    {
        oss << "-";
    }
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * style.requiredLineIndex, entryFontSize, style.textColor);

    oss.str("");
    oss << "Minerals/turn: " << m_rSnapshot.mineralProduction;
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * style.productionLineIndex, entryFontSize, style.textColor);

    oss.str("");
    oss << "Turns: ";
    if (const std::optional<int> turns = m_rBase.GetTurnsToProductionCompletion())
    {
        oss << *turns;
    }
    else
    {
        oss << "-";
    }
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * style.turnsLineIndex, entryFontSize, style.textColor);
}

void ProductionDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button == MouseButton_t::Left && m_onClicked)
    {
        m_onClicked();
    }
}

} // namespace ac
