#include "ui/base/BaseDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "graphics/Graphics.h"
#include <string>

namespace ac
{

BaseDisplay::BaseDisplay(const BaseManager& rBase, Graphics& rGraphics)
    : m_rBase(rBase)
    , m_rGraphics(rGraphics)
{
}

void BaseDisplay::SetLastClickedTileText(const std::string& text)
{
    m_lastClickedTileText = text;
}

void BaseDisplay::Render(Graphics& /*rGraphics*/)
{
    m_rGraphics.DrawText(m_rBase.GetName(), kTextX, kNameY, 20, Color::Yellow());
    m_rGraphics.DrawText("Nutrients: " + std::to_string(m_rBase.GetNutrientStockpile()), kTextX, kNutrientsY, 16, Color::White());
    m_rGraphics.DrawText("Minerals:  " + std::to_string(m_rBase.GetMineralStockpile()), kTextX, kMineralsY, 16, Color::White());
    m_rGraphics.DrawText("Energy:    " + std::to_string(m_rBase.GetEnergyProduction()) + "/turn", kTextX, kEnergyY, 16, Color::White());
    if (!m_lastClickedTileText.empty())
    {
        m_rGraphics.DrawText(m_lastClickedTileText, kTextX, kStatusY, 18, Color::Yellow());
    }
}

} // namespace ac
