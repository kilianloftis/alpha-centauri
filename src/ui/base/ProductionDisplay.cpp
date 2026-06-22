#include "ui/base/ProductionDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "graphics/Graphics.h"
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

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        Color{20, 20, 20, 255}
    );

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * k_HeaderFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(m_layout.height * k_EntryFontSizeRatio);
    const float lineHeight   = m_layout.height * k_LineHeightRatio;
    const float leftPadding  = m_layout.width  * k_LeftPaddingRatio;

    const std::string& currentProduction = m_pBase->GetProduction();
    const std::string header = currentProduction.empty() ? "Production: (none)" : "Production: " + currentProduction;
    rGraphics.DrawText(header, m_layout.x + leftPadding, m_layout.y, headerFontSize);

    std::ostringstream oss;

    oss << "Stockpile: " << m_pBase->GetMineralStockpile();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight, entryFontSize);

    oss.str("");
    oss << "Required: ";
    if (!currentProduction.empty())
    {
        oss << m_pBase->GetProductionMineralCost();
    }
    else
    {
        oss << "-";
    }
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * 2.0f, entryFontSize);

    oss.str("");
    oss << "Minerals/turn: " << m_pBase->GetMineralProduction();
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * 3.0f, entryFontSize);
}

void ProductionDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button == MouseButton_t::Left && m_onClicked)
    {
        m_onClicked();
    }
}

} // namespace ac
