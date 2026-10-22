#include "ui/base/ProductionDisplay.h"
#include "game/buildings/BuildingConfig.h"
#include "game/base/BaseManager.h"
#include "game/production/ProductionManager.h"
#include "graphics/Graphics.h"
#include "ui/SpriteLibrary.h"
#include "ui/base/BaseDisplaySnapshot.h"
#include "ui/style/DrawPanelChrome.h"
#include "ui/style/UiStyle.h"
#include <functional>
#include <optional>
#include <string>

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
    DrawHeader_(rGraphics);

    const std::string required =
        m_rSnapshot.bHasProduction ? std::to_string(m_rSnapshot.mineralCost) : "-";
    const std::optional<int> turns = m_rBase.GetTurnsToProductionCompletion();
    const std::string turnsText = turns ? std::to_string(*turns) : "-";
    const int stockpile = m_rBase.GetProduction().GetMineralStockpile();

    DrawEntry_(rGraphics, "Stockpile: " + std::to_string(stockpile), style.stockpileLineIndex);
    DrawEntry_(rGraphics, "Required: " + required, style.requiredLineIndex);
    DrawEntry_(rGraphics, "Minerals/turn: " + std::to_string(m_rSnapshot.mineralProduction),
               style.productionLineIndex);
    DrawEntry_(rGraphics, "Turns: " + turnsText, style.turnsLineIndex);
}

void ProductionDisplay::DrawHeader_(Graphics& rGraphics) const
{
    const auto& style = Style().productionDisplay;
    const unsigned int fontSize =
        static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);
    const float iconSize = m_layout.height * style.iconSizeRatio;
    const float iconGap = m_layout.width * style.iconGapRatio;

    float headerX = m_layout.x + m_layout.width * style.leftPaddingRatio;
    const IConstructable* pProduction = m_rBase.GetProduction().GetCurrentProduction();
    const auto* pBuilding = dynamic_cast<const BuildingConfig_t*>(pProduction);
    if (pBuilding && !pBuilding->icon.empty() && style.iconSizeRatio > 0.0f
        && m_rSprites.Ensure(pBuilding->icon))
    {
        rGraphics.DrawSprite(pBuilding->icon, headerX, m_layout.y, iconSize, iconSize);
        headerX += iconSize + iconGap;
    }

    const std::string header = m_rSnapshot.bHasProduction
                                   ? "Production: " + m_rSnapshot.productionName
                                   : "Production: (none)";
    rGraphics.DrawText(header, headerX, m_layout.y, fontSize, style.textColor);
}

void ProductionDisplay::DrawEntry_(Graphics& rGraphics,
                                   const std::string& rText,
                                   float lineIndex) const
{
    const auto& style = Style().productionDisplay;
    const unsigned int fontSize =
        static_cast<unsigned int>(m_layout.height * style.entryFontSizeRatio);
    const float x = m_layout.x + m_layout.width * style.leftPaddingRatio;
    const float y = m_layout.y + m_layout.height * style.lineHeightRatio * lineIndex;
    rGraphics.DrawText(rText, x, y, fontSize, style.textColor);
}

void ProductionDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button == MouseButton_t::Left && m_onClicked)
    {
        m_onClicked();
    }
}

} // namespace ac
