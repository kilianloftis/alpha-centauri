#include "ui/world/UnitStackPanel.h"

#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "graphics/Graphics.h"
#include "ui/world/UnitMarkerRenderer.h"

#include <algorithm>

namespace ac
{

namespace
{

constexpr Color_t k_BackgroundColor {20, 20, 40, 255};
constexpr Color_t k_BorderColor     {100, 100, 160, 255};
constexpr float k_PaddingRatio    = 0.04f;
constexpr float k_SlotGapRatio    = 0.02f;
constexpr float k_IconHeightRatio = 0.55f;
constexpr float k_StatFontRatio   = 0.22f;

} // namespace

UnitStackPanel::UnitStackPanel(WindowLayout_t layout, UnitClickCallback_t onUnitClicked)
    : UIElement(layout)
    , m_onUnitClicked(std::move(onUnitClicked))
{
}

void UnitStackPanel::SetUnits(std::vector<Unit*> units, const Unit* pSelectedUnit)
{
    m_units = std::move(units);
    m_pSelectedUnit = pSelectedUnit;
    CacheSlots_();
}

void UnitStackPanel::CacheSlots_()
{
    m_slots.clear();
    if (m_units.empty())
    {
        return;
    }

    const float padding = m_layout.height * k_PaddingRatio;
    const float gap = m_layout.width * k_SlotGapRatio;
    const float iconSize = m_layout.height * k_IconHeightRatio;
    const unsigned int statFontSize =
        std::max(1u, static_cast<unsigned int>(m_layout.height * k_StatFontRatio));
    const float slotWidth = iconSize;
    const float slotHeight = iconSize + static_cast<float>(statFontSize) + padding;

    float x = m_layout.x + padding;
    const float y = m_layout.y + (m_layout.height - slotHeight) * 0.5f;
    const float right = m_layout.x + m_layout.width - padding;

    for (Unit* pUnit : m_units)
    {
        if (!pUnit || x + slotWidth > right)
        {
            break;
        }
        m_slots.push_back(Slot_t{Rectangle_t{x, y, slotWidth, slotHeight}, pUnit});
        x += slotWidth + gap;
    }
}

void UnitStackPanel::Render(Graphics& rGraphics)
{
    DrawBackground_(rGraphics);
    for (const Slot_t& rSlot : m_slots)
    {
        DrawSlot_(rGraphics, rSlot);
    }
}

void UnitStackPanel::DrawBackground_(Graphics& rGraphics) const
{
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BackgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BorderColor);
}

void UnitStackPanel::DrawSlot_(Graphics& rGraphics, const Slot_t& rSlot) const
{
    const float iconSize = rSlot.rect.width;
    const Rectangle_t marker{rSlot.rect.x, rSlot.rect.y, iconSize, iconSize};
    UnitMarkerRenderer::DrawMarker(
        rGraphics, *rSlot.pUnit, marker, rSlot.pUnit == m_pSelectedUnit);

    const unsigned int statFontSize =
        std::max(1u, static_cast<unsigned int>(m_layout.height * k_StatFontRatio));
    rGraphics.DrawText(
        rSlot.pUnit->GetDesign().FormatCombatRating(),
        rSlot.rect.x,
        rSlot.rect.y + iconSize,
        statFontSize,
        Color_t::White());
}

void UnitStackPanel::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (!m_onUnitClicked)
    {
        return;
    }

    for (const Slot_t& rSlot : m_slots)
    {
        if (ContainsMouseCoord(rSlot.rect, rEvent))
        {
            m_onUnitClicked(*rSlot.pUnit);
            return;
        }
    }
}

} // namespace ac
