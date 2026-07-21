#include "ui/world/SelectedUnitPanel.h"
#include "game/faction/base/BaseManager.h"
#include "game/units/MovementConstants.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "graphics/Graphics.h"
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

SelectedUnitPanel::SelectedUnitPanel(WindowLayout_t layout)
    : UIElement(layout)
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
    textY = DrawStats_(rGraphics, textX, textY, fontSize) + textGap;
    textY = DrawMoves_(rGraphics, textX, textY, fontSize) + textGap;
    textY = DrawOrders_(rGraphics, textX, textY, fontSize) + textGap;
    DrawHomeBase_(rGraphics, textX, textY, fontSize);
}

void SelectedUnitPanel::DrawBackground_(Graphics& rGraphics) const
{
    const auto& s = Style().selectedUnitPanel;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.borderColor);
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
