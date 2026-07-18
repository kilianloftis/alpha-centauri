#include "ui/world/SelectedUnitPanel.h"
#include "game/faction/base/BaseManager.h"
#include "game/units/MovementConstants.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "graphics/Graphics.h"
#include <algorithm>
#include <sstream>
#include <string>

namespace ac
{

namespace
{

constexpr Color_t k_BackgroundColor        {20, 20, 40, 255};
constexpr Color_t k_BorderColor            {100, 100, 160, 255};
constexpr Color_t k_MutedTextColor         {100, 100, 120, 255};
constexpr Color_t k_IconColor              {0, 220, 255, 255};
constexpr Color_t k_IconExhaustedColor     {0, 110, 130, 200};
constexpr float k_PaddingRatio           = 0.06f;
constexpr float k_IconSizeRatio          = 0.45f;
constexpr float k_IconInitialFontRatio   = 0.45f;
constexpr float k_TextFontRatio          = 0.06f;
constexpr float k_TextGapRatio           = 0.04f;
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

    const float padding = m_layout.width * k_PaddingRatio;
    const unsigned int fontSize =
        static_cast<unsigned int>(m_layout.height * k_TextFontRatio);
    const float textGap = m_layout.height * k_TextGapRatio;
    const float iconSize = std::min(m_layout.width, m_layout.height) * k_IconSizeRatio;
    const float iconX = m_layout.x + (m_layout.width - iconSize) * 0.5f;
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
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BackgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BorderColor);
}

void SelectedUnitPanel::DrawEmptyState_(Graphics& rGraphics) const
{
    const float padding = m_layout.width * k_PaddingRatio;
    const unsigned int fontSize =
        static_cast<unsigned int>(m_layout.height * k_TextFontRatio);
    rGraphics.DrawText(
        "No unit",
        m_layout.x + padding,
        m_layout.y + padding,
        fontSize,
        k_MutedTextColor);
}

void SelectedUnitPanel::DrawUnitIcon_(Graphics& rGraphics, float iconX, float iconY, float iconSize) const
{
    const bool bExhausted = m_pSelectedUnit->GetMoveFragmentsRemaining() <= 0;
    const Color_t& rIconColor = bExhausted ? k_IconExhaustedColor : k_IconColor;
    rGraphics.DrawFilledRect(iconX, iconY, iconSize, iconSize, rIconColor);
    rGraphics.DrawRect(iconX, iconY, iconSize, iconSize, k_BorderColor);

    const std::string& unitName = m_pSelectedUnit->GetDesign().GetName();
    if (unitName.empty())
    {
        return;
    }

    const unsigned int initialFontSize =
        static_cast<unsigned int>(iconSize * k_IconInitialFontRatio);
    rGraphics.DrawText(
        unitName.substr(0, k_UnitNameFirstCharCount),
        iconX + iconSize * 0.3f,
        iconY + iconSize * 0.25f,
        initialFontSize,
        Color_t::Black());
}

float SelectedUnitPanel::DrawName_(Graphics& rGraphics, float textX, float textY,
                                   unsigned int fontSize) const
{
    rGraphics.DrawText(
        m_pSelectedUnit->GetDesign().GetName(),
        textX,
        textY,
        fontSize,
        Color_t::White());
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
        Color_t::White());
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

    rGraphics.DrawText(oss.str(), textX, textY, fontSize, Color_t::White());
    return textY + static_cast<float>(fontSize);
}

float SelectedUnitPanel::DrawOrders_(Graphics& rGraphics, float textX, float textY,
                                     unsigned int fontSize) const
{
    const std::optional<UnitOrder_t>& rOrder = m_pSelectedUnit->GetOrder();
    const std::string orderText = "Orders: " + (rOrder ? ToString(*rOrder) : "None");
    rGraphics.DrawText(
        orderText,
        textX,
        textY,
        fontSize,
        rOrder ? Color_t::White() : k_MutedTextColor);
    return textY + static_cast<float>(fontSize);
}

void SelectedUnitPanel::DrawHomeBase_(Graphics& rGraphics, float textX, float textY,
                                      unsigned int fontSize) const
{
    const BaseManager* pHomeBase = m_pSelectedUnit->GetHomeBase();
    const std::string homeText = "Home: " + (pHomeBase ? pHomeBase->GetName() : "None");
    rGraphics.DrawText(
        homeText,
        textX,
        textY,
        fontSize,
        pHomeBase ? Color_t::White() : k_MutedTextColor);
}

} // namespace ac
