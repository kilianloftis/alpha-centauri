#include "ui/base/BuildingsDisplay.h"

#include "game/buildings/BuildingConfig.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "graphics/Graphics.h"
#include "ui/SpriteLibrary.h"
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

BuildingsDisplay::BuildingsDisplay(const BaseManager& rBase,
                                   SpriteLibrary& rSprites,
                                   WindowLayout_t layout,
                                   BuildingClickCallback_t onBuildingClick)
    : UIElement(layout)
    , m_rBase(rBase)
    , m_rSprites(rSprites)
    , m_onBuildingClick(std::move(onBuildingClick))
{
}

std::vector<BuildingsDisplay::BuildingRow_t> BuildingsDisplay::Rows_() const
{
    const auto& style = Style().buildingsDisplay;
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

    const float lineHeight = m_layout.height * style.lineHeightRatio;
    const float bottom = m_layout.y + m_layout.height;
    float y = m_layout.y + lineHeight * style.headerLineOffset;

    std::vector<BuildingRow_t> rows;
    for (const BuildingConfig_t* pBuilding : rBuildings.GetBuildings())
    {
        if (!pBuilding)
        {
            continue;
        }
        if (y + lineHeight > bottom)
        {
            return rows;
        }
        const bool bGranted = grantedIds.count(pBuilding->id) != 0;
        BuildingRow_t row;
        row.pConfig = pBuilding;
        row.label = bGranted ? std::string(k_GrantedAndConstructedPrefix) + pBuilding->GetName()
                             : pBuilding->GetName();
        row.color = bGranted ? style.grantedTextColor : style.textColor;
        row.bounds = WindowLayout_t{m_layout.x, y, m_layout.width, lineHeight};
        rows.push_back(std::move(row));
        y += lineHeight;
    }

    for (const BuildingConfig_t* pGranted : granted)
    {
        if (!pGranted || rBuildings.HasBuilding(pGranted->id))
        {
            continue;
        }
        if (y + lineHeight > bottom)
        {
            break;
        }
        BuildingRow_t row;
        row.pConfig = pGranted;
        row.label = pGranted->GetName();
        row.color = style.grantedTextColor;
        row.bounds = WindowLayout_t{m_layout.x, y, m_layout.width, lineHeight};
        rows.push_back(std::move(row));
        y += lineHeight;
    }
    return rows;
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
    const float leftPadding = m_layout.width * style.leftPaddingRatio;
    const float lineHeight = m_layout.height * style.lineHeightRatio;
    const float iconSize = lineHeight * style.iconSizeRatio;
    const float iconGap = m_layout.width * style.iconGapRatio;

    rGraphics.DrawText(
        "Buildings", m_layout.x + leftPadding, m_layout.y, headerFontSize, style.textColor);

    for (const BuildingRow_t& rRow : Rows_())
    {
        float textX = rRow.bounds.x + leftPadding;
        if (rRow.pConfig && !rRow.pConfig->icon.empty()
            && m_rSprites.Ensure(rRow.pConfig->icon))
        {
            const float iconY = rRow.bounds.y + (rRow.bounds.height - iconSize) * 0.5f;
            rGraphics.DrawSprite(rRow.pConfig->icon, textX, iconY, iconSize, iconSize);
            textX += iconSize + iconGap;
        }
        rGraphics.DrawText(rRow.label, textX, rRow.bounds.y, entryFontSize, rRow.color);
    }
}

void BuildingsDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (!m_onBuildingClick || rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    for (const BuildingRow_t& rRow : Rows_())
    {
        if (ContainsMouseCoord(rRow.bounds, rEvent) && rRow.pConfig)
        {
            m_onBuildingClick(*rRow.pConfig);
            return;
        }
    }
}

} // namespace ac
