#include "ui/base/ProductionDisplay.h"
#include "game/buildings/BuildingConfig.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "graphics/Graphics.h"
#include "ui/SpriteLibrary.h"
#include "ui/base/BaseDisplaySnapshot.h"
#include "ui/style/DrawPanelChrome.h"
#include "ui/style/UiStyle.h"
#include <functional>
#include <optional>
#include <sstream>

namespace ac
{

ProductionDisplay::ProductionDisplay(
    const BaseManager& rBase,
    const BaseDisplaySnapshot_t& rSnapshot,
    SpriteLibrary& rSprites,
    WindowLayout_t layout,
    std::function<void()> onClicked
)
    : UIElement(layout)
    , m_onClicked(std::move(onClicked))
    , m_rBase(rBase)
    , m_rSnapshot(rSnapshot)
    , m_rSprites(rSprites)
{}

void ProductionDisplay::Render(Graphics& rGraphics)
{
    const auto& style = Style().productionDisplay;

    DrawPanelChrome(rGraphics, &m_rSprites, m_layout, style.backgroundColor,
                    style.backgroundSprite, Color_t::Black(), 0.0f);

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(m_layout.height * style.entryFontSizeRatio);
    const float lineHeight   = m_layout.height * style.lineHeightRatio;
    const float leftPadding  = m_layout.width  * style.leftPaddingRatio;
    const float iconSize = m_layout.height * style.iconSizeRatio;
    const float iconGap = m_layout.width * style.iconGapRatio;

    float headerX = m_layout.x + leftPadding;
    const IConstructable* pProduction = m_rBase.GetProduction().GetCurrentProduction();
    if (const auto* pBuilding = dynamic_cast<const BuildingConfig_t*>(pProduction))
    {
        if (!pBuilding->icon.empty() && style.iconSizeRatio > 0.0f
            && m_rSprites.Ensure(pBuilding->icon))
        {
            rGraphics.DrawSprite(pBuilding->icon, headerX, m_layout.y, iconSize, iconSize);
            headerX += iconSize + iconGap;
        }
    }

    const std::string header = m_rSnapshot.bHasProduction
                                   ? "Production: " + m_rSnapshot.productionName
                                   : "Production: (none)";
    rGraphics.DrawText(header, headerX, m_layout.y, headerFontSize, style.textColor);

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
