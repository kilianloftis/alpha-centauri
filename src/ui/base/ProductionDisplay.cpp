#include "ui/base/ProductionDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "graphics/Graphics.h"
#include <functional>
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

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        k_BackgroundColor
    );

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * k_HeaderFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(m_layout.height * k_EntryFontSizeRatio);
    const float lineHeight   = m_layout.height * k_LineHeightRatio;
    const float leftPadding  = m_layout.width  * k_LeftPaddingRatio;

    const ProductionManager& rProduction = m_pBase->GetProduction();
    const IConstructable* pCurrentProduction = rProduction.GetCurrentProduction();
    const std::string header = pCurrentProduction ? "Production: " + pCurrentProduction->GetName() : "Production: (none)";
    rGraphics.DrawText(header, m_layout.x + leftPadding, m_layout.y, headerFontSize);

    std::ostringstream oss;

    oss << "Stockpile: " << rProduction.GetMineralStockpile();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * k_StockpileLineIndex, entryFontSize);

    oss.str("");
    oss << "Required: ";
    if (pCurrentProduction)
    {
        oss << rProduction.GetMineralCost();
    }
    else
    {
        oss << "-";
    }
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * k_RequiredLineIndex, entryFontSize);

    oss.str("");
    oss << "Minerals/turn: " << m_pBase->GetMineralProduction();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * k_ProductionLineIndex, entryFontSize);
}

void ProductionDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button == MouseButton_t::Left && m_onClicked)
    {
        m_onClicked();
    }
}

} // namespace ac
