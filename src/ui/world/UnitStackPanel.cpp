#include "ui/world/UnitStackPanel.h"

#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"
#include "ui/world/UnitMarkerRenderer.h"

#include <algorithm>
#include <string>

namespace ac
{

UnitStackPanel::UnitStackPanel(WindowLayout_t layout, UnitClickCallback_t onUnitClicked)
    : UIElement(layout)
    , m_onUnitClicked(std::move(onUnitClicked))
{
}

void UnitStackPanel::SetUnits(std::vector<Unit*> units, const Unit* pSelectedUnit)
{
    m_units = std::move(units);
    m_pSelectedUnit = pSelectedUnit;
    m_scrollOffset = 0;
    CacheSlots_();
    ScrollSelectedIntoView_();
}

void UnitStackPanel::CacheSlots_()
{
    m_slots.clear();
    if (m_units.empty())
    {
        return;
    }

    const auto& s = Style().unitStackPanel;
    const float padding = m_layout.height * s.paddingRatio;
    const float gap = m_layout.width * s.slotGapRatio;
    const float iconSize = m_layout.height * s.iconHeightRatio;
    const unsigned int statFontSize =
        std::max(1u, static_cast<unsigned int>(m_layout.height * s.statFontRatio));
    const float slotWidth = iconSize;
    const float slotHeight = iconSize + static_cast<float>(statFontSize) + padding;

    float x = m_layout.x + padding;
    const float y = m_layout.y + (m_layout.height - slotHeight) * 0.5f;
    const float right = m_layout.x + m_layout.width - padding;

    for (size_t i = m_scrollOffset; i < m_units.size(); ++i)
    {
        Unit* pUnit = m_units[i];
        if (!pUnit || x + slotWidth > right)
        {
            break;
        }
        m_slots.push_back(Slot_t{Rectangle_t{x, y, slotWidth, slotHeight}, pUnit});
        x += slotWidth + gap;
    }
}

bool UnitStackPanel::HasHiddenUnits() const
{
    return m_scrollOffset > 0 || m_scrollOffset + m_slots.size() < m_units.size();
}

// Every unit on the tile stays reachable through the existing select-next-unit cycle: the
// window follows the selection instead of the tail of the stack being unclickable.
void UnitStackPanel::ScrollSelectedIntoView_()
{
    if (!m_pSelectedUnit || m_slots.empty())
    {
        return;
    }

    const auto it = std::find(m_units.begin(), m_units.end(), m_pSelectedUnit);
    if (it == m_units.end())
    {
        return;
    }

    const size_t selectedIndex = static_cast<size_t>(std::distance(m_units.begin(), it));
    const size_t visible = m_slots.size();
    if (selectedIndex < m_scrollOffset + visible)
    {
        return;
    }

    m_scrollOffset = selectedIndex - visible + 1;
    CacheSlots_();
}

void UnitStackPanel::Render(Graphics& rGraphics)
{
    DrawBackground_(rGraphics);
    for (const Slot_t& rSlot : m_slots)
    {
        DrawSlot_(rGraphics, rSlot);
    }

    if (HasHiddenUnits())
    {
        const auto& s = Style().unitStackPanel;
        const auto fontSize =
            std::max(1u, static_cast<unsigned int>(m_layout.height * s.statFontRatio));
        const std::string cue = std::to_string(m_scrollOffset + 1) + "-"
                                + std::to_string(m_scrollOffset + m_slots.size()) + "/"
                                + std::to_string(m_units.size());
        rGraphics.DrawText(cue, m_layout.x + m_layout.width * s.paddingRatio,
                           m_layout.y + m_layout.height * s.paddingRatio, fontSize,
                           s.statTextColor);
    }
}

void UnitStackPanel::DrawBackground_(Graphics& rGraphics) const
{
    const auto& s = Style().unitStackPanel;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.borderColor);
}

void UnitStackPanel::DrawSlot_(Graphics& rGraphics, const Slot_t& rSlot) const
{
    const auto& s = Style().unitStackPanel;
    const float iconSize = rSlot.rect.width;
    const Rectangle_t marker{rSlot.rect.x, rSlot.rect.y, iconSize, iconSize};
    UnitMarkerRenderer::DrawMarker(
        rGraphics, *rSlot.pUnit, marker, rSlot.pUnit == m_pSelectedUnit);

    const unsigned int statFontSize =
        std::max(1u, static_cast<unsigned int>(m_layout.height * s.statFontRatio));
    std::string label = rSlot.pUnit->GetDesign().FormatCombatRating();
    if (rSlot.pUnit->IsEmbarked())
    {
        label += " c";
    }
    rGraphics.DrawText(
        label,
        rSlot.rect.x,
        rSlot.rect.y + iconSize,
        statFontSize,
        s.statTextColor);
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
