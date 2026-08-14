#include "ui/base/BuildingsDisplay.h"

#include "game/buildings/BuildingConfig.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace ac
{

namespace
{

constexpr const char* k_GrantedAndConstructedPrefix = "* ";

} // namespace

BuildingsDisplay::BuildingsDisplay(const BaseManager& rBase, WindowLayout_t layout)
    : UIElement(layout)
    , m_rBase(rBase)
{
}

void BuildingsDisplay::Render(Graphics& rGraphics)
{
    const auto& style = Style().buildingsDisplay;

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);

    const unsigned int headerFontSize =
        static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);
    const unsigned int entryFontSize =
        static_cast<unsigned int>(m_layout.height * style.entryFontSizeRatio);
    const float lineHeight = m_layout.height * style.lineHeightRatio;
    const float leftPadding = m_layout.width * style.leftPaddingRatio;
    const float bottom = m_layout.y + m_layout.height;

    rGraphics.DrawText(
        "Buildings", m_layout.x + leftPadding, m_layout.y, headerFontSize, style.textColor);

    const BuildingManager& rBuildings = m_rBase.GetBuildingManager();
    const std::vector<const BuildingConfig_t*> granted = m_rBase.GetGrantedBuildings();
    std::unordered_set<BuildingId_t> grantedIds;
    for (const BuildingConfig_t* pGranted : granted)
    {
        if (pGranted)
        {
            grantedIds.insert(pGranted->id);
        }
    }

    float y = m_layout.y + lineHeight * style.headerLineOffset;
    auto drawRow = [&](const std::string& rLabel, const Color_t& rColor) {
        if (y + lineHeight > bottom)
        {
            return false;
        }
        rGraphics.DrawText(rLabel, m_layout.x + leftPadding, y, entryFontSize, rColor);
        y += lineHeight;
        return true;
    };

    for (const BuildingConfig_t* pBuilding : rBuildings.GetBuildings())
    {
        if (!pBuilding)
        {
            continue;
        }
        const bool bGranted = grantedIds.count(pBuilding->id) != 0;
        const std::string label =
            bGranted ? std::string(k_GrantedAndConstructedPrefix) + pBuilding->GetName()
                     : pBuilding->GetName();
        const Color_t& rColor = bGranted ? style.grantedTextColor : style.textColor;
        if (!drawRow(label, rColor))
        {
            return;
        }
    }

    for (const BuildingConfig_t* pGranted : granted)
    {
        if (!pGranted || rBuildings.HasBuilding(pGranted->id))
        {
            continue;
        }
        if (!drawRow(pGranted->GetName(), style.grantedTextColor))
        {
            return;
        }
    }
}

} // namespace ac
