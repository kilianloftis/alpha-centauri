#include "ui/unit-designer/SlotColumnPanel.h"
#include "game/units/UnitComponentConfig.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "ui/style/UiStyle.h"

namespace ac
{

SlotColumnPanel::SlotColumnPanel(std::vector<SlotEntry_t> slots, WindowLayout_t layout)
    : UIElement(layout)
    , m_slots(std::move(slots))
{
    CacheRects_();
}

bool SlotColumnPanel::NeedsScroll_() const
{
    return static_cast<int>(m_slots.size()) > Style().slotColumnPanel.visibleSlots;
}

void SlotColumnPanel::CacheRects_()
{
    m_slotRects.clear();

    if (NeedsScroll_())
    {
        const float arrowH = m_layout.height * Style().slotColumnPanel.arrowHeightRatio;
        const float slotAreaY = m_layout.y + arrowH;
        const float slotAreaH = m_layout.height - arrowH * Style().slotColumnPanel.arrowAreaMultiplier;
        const float slotH = slotAreaH / static_cast<float>(Style().slotColumnPanel.visibleSlots);

        m_upArrowRect   = {m_layout.x, m_layout.y,                          m_layout.width, arrowH};
        m_downArrowRect = {m_layout.x, m_layout.y + m_layout.height - arrowH, m_layout.width, arrowH};

        for (int i = 0; i < Style().slotColumnPanel.visibleSlots; ++i)
        {
            m_slotRects.push_back({
                m_layout.x,
                slotAreaY + static_cast<float>(i) * slotH,
                m_layout.width,
                slotH
            });
        }
    }
    else
    {
        const int count = static_cast<int>(m_slots.size());
        if (count == 0) return;
        const float slotH = m_layout.height / static_cast<float>(count);
        for (int i = 0; i < count; ++i)
        {
            m_slotRects.push_back({
                m_layout.x,
                m_layout.y + static_cast<float>(i) * slotH,
                m_layout.width,
                slotH
            });
        }
    }
}

void SlotColumnPanel::Render(Graphics& rGraphics)
{
    if (NeedsScroll_())
    {
        const bool bCanScrollUp   = m_scrollOffset > 0;
        const bool bCanScrollDown =
            m_scrollOffset + Style().slotColumnPanel.visibleSlots < static_cast<int>(m_slots.size());

        const Color_t upColor   = bCanScrollUp
            ? Style().slotColumnPanel.enabledArrowColor
            : Style().slotColumnPanel.disabledArrowColor;
        const Color_t downColor = bCanScrollDown
            ? Style().slotColumnPanel.enabledArrowColor
            : Style().slotColumnPanel.disabledArrowColor;

        rGraphics.DrawFilledRect(
            m_upArrowRect.x, m_upArrowRect.y,
            m_upArrowRect.width, m_upArrowRect.height,
            Style().slotColumnPanel.arrowFillColor
        );
        rGraphics.DrawRect(
            m_upArrowRect.x, m_upArrowRect.y,
            m_upArrowRect.width, m_upArrowRect.height,
            Style().slotColumnPanel.arrowBorderColor
        );
        const unsigned int arrowFontSize = static_cast<unsigned int>(
            m_upArrowRect.height * Style().slotColumnPanel.arrowFontSizeRatio);
        const float arrowPadX = m_upArrowRect.width * Style().slotColumnPanel.arrowPadXRatio;
        rGraphics.DrawText("^", m_upArrowRect.x + arrowPadX, m_upArrowRect.y, arrowFontSize, upColor);

        rGraphics.DrawFilledRect(
            m_downArrowRect.x, m_downArrowRect.y,
            m_downArrowRect.width, m_downArrowRect.height,
            Style().slotColumnPanel.arrowFillColor
        );
        rGraphics.DrawRect(
            m_downArrowRect.x, m_downArrowRect.y,
            m_downArrowRect.width, m_downArrowRect.height,
            Style().slotColumnPanel.arrowBorderColor
        );
        rGraphics.DrawText("v", m_downArrowRect.x + arrowPadX, m_downArrowRect.y, arrowFontSize, downColor);
    }

    const int visibleCount = std::min(
        Style().slotColumnPanel.visibleSlots, static_cast<int>(m_slots.size()));
    for (int i = 0; i < visibleCount; ++i)
    {
        const int slotIdx = m_scrollOffset + i;
        if (slotIdx >= static_cast<int>(m_slots.size())) break;

        const SlotEntry_t& rEntry = m_slots[slotIdx];
        const Rectangle_t& rRect = m_slotRects[i];

        rGraphics.DrawFilledRect(
            rRect.x, rRect.y, rRect.width, rRect.height,
            Style().slotColumnPanel.slotFillColor
        );
        rGraphics.DrawRect(
            rRect.x, rRect.y, rRect.width, rRect.height,
            Style().slotColumnPanel.slotBorderColor
        );

        const float padding = rRect.height * Style().slotColumnPanel.paddingRatio;
        const unsigned int labelSize = static_cast<unsigned int>(
            rRect.height * Style().slotColumnPanel.labelFontSizeRatio);
        const unsigned int nameSize = static_cast<unsigned int>(
            rRect.height * Style().slotColumnPanel.nameFontSizeRatio);

        const bool bRequired = rEntry.pSlotConfig->required;
        const std::string label = rEntry.pSlotConfig->displayName + (bRequired ? "" : " (opt)");
        rGraphics.DrawText(
            label, rRect.x + padding, rRect.y + padding, labelSize,
            Style().slotColumnPanel.labelTextColor
        );

        const UnitComponentConfig_t* pComp = rEntry.getComponent();
        const std::string& rName = pComp ? pComp->name : "(none)";
        const Color_t nameColor = pComp
            ? Style().slotColumnPanel.filledNameColor
            : Style().slotColumnPanel.emptyNameColor;
        rGraphics.DrawText(
            rName,
            rRect.x + padding,
            rRect.y + padding + static_cast<float>(labelSize)
                * Style().slotColumnPanel.nameLabelSpacingMultiplier,
            nameSize,
            nameColor
        );
    }
}

void SlotColumnPanel::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    if (NeedsScroll_())
    {
        if (ContainsMouseCoord(m_upArrowRect, rEvent) && m_scrollOffset > 0)
        {
            --m_scrollOffset;
            return;
        }

        if (ContainsMouseCoord(m_downArrowRect, rEvent) &&
            m_scrollOffset + Style().slotColumnPanel.visibleSlots < static_cast<int>(m_slots.size()))
        {
            ++m_scrollOffset;
            return;
        }
    }

    const int visibleCount = std::min(
        Style().slotColumnPanel.visibleSlots, static_cast<int>(m_slots.size()));
    for (int i = 0; i < visibleCount; ++i)
    {
        if (ContainsMouseCoord(m_slotRects[i], rEvent))
        {
            const int slotIdx = m_scrollOffset + i;
            if (slotIdx < static_cast<int>(m_slots.size()) && m_slots[slotIdx].onClicked)
            {
                m_slots[slotIdx].onClicked();
            }
            return;
        }
    }
}

} // namespace ac
