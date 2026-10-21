#include "ui/world/SelectedUnitPanel.h"
#include "game/base/BaseManager.h"
#include "game/combat/MoraleCalculator.h"
#include "game/combat/MoraleConfig.h"
#include "game/combat/MoraleRules.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/map/ImprovementIds.h"
#include "game/map/Tile.h"
#include "game/movement/MovementConstants.h"
#include "game/population/PopulationManager.h"
#include "game/social-engineering/SocialEngineeringManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "graphics/Graphics.h"
#include "ui/SpriteLibrary.h"
#include "ui/style/DrawPanelChrome.h"
#include "ui/style/UiStyle.h"
#include <algorithm>
#include <sstream>
#include <string>

namespace ac
{

namespace
{

constexpr size_t k_UnitNameFirstCharCount = 1;

} // namespace

std::string FormatMoraleDisplay(const Unit& rUnit, const MoraleCalculator& rMorale)
{
    EffectContext_t displayCtx{&rUnit.GetTile()};
    displayCtx.pUnit = &rUnit;
    std::string text = MoraleDisplayName(rUnit, displayCtx, rMorale);

    const int rank = EffectiveMoraleLevel(rUnit, displayCtx, rMorale);
    const MoraleConfig_t& rConfig = rMorale.GetConfig();
    const bool bOnBase = rUnit.GetTile().HasFeature(ImprovementIds::k_Base);
    const bool bCountInSquare = rank < rConfig.MaxLevel() && bOnBase;
    int pluses = 0;
    if (bCountInSquare)
    {
        // Defender extras are the in-square facility marks; attack penalties stay off the string.
        EffectContext_t extraCtx = displayCtx;
        extraCtx.combatRole = CombatRole_t::Defender;
        const int extras = ResolveStat(rUnit, StatId_t::MoraleExtra, extraCtx);
        pluses += std::max(0, extras);
    }
    const Faction& rFaction = rUnit.GetFaction();
    const SocialEngineeringManager& rSe = rFaction.GetSocialEngineering();
    const int seMorale = rSe.GetSocialRating(SocialRatingId_t::Morale);
    // SE +2/+3 is a field mark; on a base it is already in the defender extras above.
    if ((seMorale == 2 || seMorale == 3) && !bCountInSquare)
    {
        ++pluses;
    }
    if (rank == rConfig.MinLevel() && pluses == 0)
    {
        pluses = 1;
    }
    text.append(static_cast<std::size_t>(pluses), '+');

    const BaseManager* pHome = rUnit.GetHomeBase();
    if (pHome != nullptr)
    {
        const PopulationManager& rPop = pHome->GetPopulation();
        if (rPop.IsRioting() && rank > rConfig.MinLevel())
        {
            text += " (-)";
        }
    }
    return text;
}

SelectedUnitPanel::SelectedUnitPanel(WindowLayout_t layout, SpriteLibrary& rSprites,
                                     const MoraleCalculator& rMorale)
    : UIElement(layout)
    , m_rSprites(rSprites)
    , m_rMorale(rMorale)
{
}

void SelectedUnitPanel::Render(Graphics& rGraphics)
{
    DrawBackground_(rGraphics);

    if (!m_pSelectedUnit)
    {
        DrawEmptyState_(rGraphics);
        return;
    }

    const auto& s = Style().selectedUnitPanel;
    const float padding = m_layout.width * s.paddingRatio;
    const unsigned int fontSize =
        static_cast<unsigned int>(m_layout.height * s.textFontRatio);
    const float textGap = m_layout.height * s.textGapRatio;
    const float iconSize = std::min(m_layout.width, m_layout.height) * s.iconSizeRatio;
    const float iconX = m_layout.x + (m_layout.width - iconSize) * s.iconCenterRatio;
    const float iconY = m_layout.y + padding;
    const float textX = m_layout.x + padding;

    DrawUnitIcon_(rGraphics, iconX, iconY, iconSize);

    float textY = iconY + iconSize + textGap;
    textY = DrawName_(rGraphics, textX, textY, fontSize) + textGap;
    textY = DrawMorale_(rGraphics, textX, textY, fontSize) + textGap;
    textY = DrawStats_(rGraphics, textX, textY, fontSize) + textGap;
    textY = DrawMoves_(rGraphics, textX, textY, fontSize) + textGap;
    {
        const float fuelY = DrawFuel_(rGraphics, textX, textY, fontSize);
        if (fuelY != textY)
        {
            textY = fuelY + textGap;
        }
    }
    textY = DrawOrders_(rGraphics, textX, textY, fontSize) + textGap;
    DrawHomeBase_(rGraphics, textX, textY, fontSize);
}

void SelectedUnitPanel::DrawBackground_(Graphics& rGraphics) const
{
    const auto& s = Style().selectedUnitPanel;
    DrawPanelChrome(rGraphics, &m_rSprites, m_layout, s.backgroundColor, s.backgroundSprite,
                    s.borderColor, 0.0f);
}

void SelectedUnitPanel::DrawEmptyState_(Graphics& rGraphics) const
{
    const auto& s = Style().selectedUnitPanel;
    const float padding = m_layout.width * s.paddingRatio;
    const unsigned int fontSize =
        static_cast<unsigned int>(m_layout.height * s.textFontRatio);
    rGraphics.DrawText(
        "No unit",
        m_layout.x + padding,
        m_layout.y + padding,
        fontSize,
        s.mutedTextColor);
}

void SelectedUnitPanel::DrawUnitIcon_(Graphics& rGraphics, float iconX, float iconY, float iconSize) const
{
    const auto& s = Style().selectedUnitPanel;
    const bool bExhausted = m_pSelectedUnit->GetMoveFragmentsRemaining() <= 0;
    const Color_t& rIconColor = bExhausted ? s.iconExhaustedColor : s.iconColor;
    rGraphics.DrawFilledRect(iconX, iconY, iconSize, iconSize, rIconColor);
    rGraphics.DrawRect(iconX, iconY, iconSize, iconSize, s.borderColor);

    const std::string& unitName = m_pSelectedUnit->GetDesign().GetName();
    if (unitName.empty())
    {
        return;
    }

    const unsigned int initialFontSize =
        static_cast<unsigned int>(iconSize * s.iconInitialFontRatio);
    rGraphics.DrawText(
        unitName.substr(0, k_UnitNameFirstCharCount),
        iconX + iconSize * s.iconInitialOffsetXRatio,
        iconY + iconSize * s.iconInitialOffsetYRatio,
        initialFontSize,
        s.iconInitialTextColor);
}

float SelectedUnitPanel::DrawName_(Graphics& rGraphics, float textX, float textY,
                                   unsigned int fontSize) const
{
    rGraphics.DrawText(
        m_pSelectedUnit->GetDesign().GetName(),
        textX,
        textY,
        fontSize,
        Style().selectedUnitPanel.bodyTextColor);
    return textY + static_cast<float>(fontSize);
}

float SelectedUnitPanel::DrawMorale_(Graphics& rGraphics, float textX, float textY,
                                     unsigned int fontSize) const
{
    const std::string text = FormatMoraleDisplay(*m_pSelectedUnit, m_rMorale);
    rGraphics.DrawText(
        text,
        textX,
        textY,
        fontSize,
        Style().selectedUnitPanel.bodyTextColor);
    return textY + static_cast<float>(fontSize);
}

float SelectedUnitPanel::DrawStats_(Graphics& rGraphics, float textX, float textY,
                                    unsigned int fontSize) const
{
    rGraphics.DrawText(
        m_pSelectedUnit->GetDesign().FormatCombatRating(),
        textX,
        textY,
        fontSize,
        Style().selectedUnitPanel.bodyTextColor);
    return textY + static_cast<float>(fontSize);
}

float SelectedUnitPanel::DrawMoves_(Graphics& rGraphics, float textX, float textY,
                                    unsigned int fontSize) const
{
    const int remainingFragments = m_pSelectedUnit->GetMoveFragmentsRemaining();
    const int maxPoints = m_pSelectedUnit->GetMovementPoints();
    const float remainingPoints =
        static_cast<float>(remainingFragments)
        / static_cast<float>(MovementConstants_t::k_moveFragmentsPerPoint);

    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(remainingFragments % MovementConstants_t::k_moveFragmentsPerPoint == 0 ? 0 : 1);
    oss << "Moves: " << remainingPoints << "/" << maxPoints;

    rGraphics.DrawText(oss.str(), textX, textY, fontSize, Style().selectedUnitPanel.bodyTextColor);
    return textY + static_cast<float>(fontSize);
}

float SelectedUnitPanel::DrawFuel_(Graphics& rGraphics, float textX, float textY,
                                   unsigned int fontSize) const
{
    if (!m_pSelectedUnit->GetDesign().UsesFuel())
    {
        return textY;
    }

    std::ostringstream oss;
    oss << "Fuel: " << m_pSelectedUnit->GetCurrentFuel() << "/" << m_pSelectedUnit->GetMaxFuel();
    rGraphics.DrawText(oss.str(), textX, textY, fontSize, Style().selectedUnitPanel.bodyTextColor);
    return textY + static_cast<float>(fontSize);
}

float SelectedUnitPanel::DrawOrders_(Graphics& rGraphics, float textX, float textY,
                                     unsigned int fontSize) const
{
    const auto& s = Style().selectedUnitPanel;
    const std::optional<UnitOrder_t>& rOrder = m_pSelectedUnit->GetOrder();
    const std::string orderText = "Orders: " + (rOrder ? ToString(*rOrder) : "None");
    rGraphics.DrawText(
        orderText,
        textX,
        textY,
        fontSize,
        rOrder ? s.bodyTextColor : s.mutedTextColor);
    return textY + static_cast<float>(fontSize);
}

void SelectedUnitPanel::DrawHomeBase_(Graphics& rGraphics, float textX, float textY,
                                      unsigned int fontSize) const
{
    const auto& s = Style().selectedUnitPanel;
    const BaseManager* pHomeBase = m_pSelectedUnit->GetHomeBase();
    const std::string homeText = "Home: " + (pHomeBase ? pHomeBase->GetName() : "None");
    rGraphics.DrawText(
        homeText,
        textX,
        textY,
        fontSize,
        pHomeBase ? s.bodyTextColor : s.mutedTextColor);
}

} // namespace ac
