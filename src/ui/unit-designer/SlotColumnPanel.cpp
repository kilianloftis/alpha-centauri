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
        DrawScrollArrows_(rGraphics);
    }

    const int visibleCount = std::min(
        Style().slotColumnPanel.visibleSlots, static_cast<int>(m_slots.size()));
    for (int i = 0; i < visibleCount; ++i)
    {
        const int slotIdx = m_scrollOffset + i;
        if (slotIdx >= static_cast<int>(m_slots.size())) break;

        DrawSlot_(rGraphics, m_slots[slotIdx], m_slotRects[i]);
    }
}

void SlotColumnPanel::DrawScrollArrows_(Graphics& rGraphics) const
{
    const auto& style = Style().slotColumnPanel;
    const bool bCanScrollUp   = m_scrollOffset > 0;
    const bool bCanScrollDown =
        m_scrollOffset + style.visibleSlots < static_cast<int>(m_slots.size());

    const Color_t upColor = bCanScrollUp ? style.enabledArrowColor : style.disabledArrowColor;
    const Color_t downColor = bCanScrollDown ? style.enabledArrowColor : style.disabledArrowColor;
    DrawArrow_(rGraphics, m_upArrowRect, "^", upColor);
    DrawArrow_(rGraphics, m_downArrowRect, "v", downColor);
}

void SlotColumnPanel::DrawArrow_(Graphics& rGraphics,
                                 const Rectangle_t& rRect,
                                 const std::string& rGlyph,
                                 Color_t glyphColor) const
{
    const auto& style = Style().slotColumnPanel;
    rGraphics.DrawFilledRect(rRect.x, rRect.y, rRect.width, rRect.height, style.arrowFillColor);
    rGraphics.DrawRect(rRect.x, rRect.y, rRect.width, rRect.height,
        style.arrowBorderColor, Style().outline.width);

    const unsigned int fontSize =
        static_cast<unsigned int>(rRect.height * style.arrowFontSizeRatio);
    const float padX = rRect.width * style.arrowPadXRatio;
    rGraphics.DrawText(rGlyph, rRect.x + padX, rRect.y, fontSize, glyphColor);
}

void SlotColumnPanel::DrawSlot_(Graphics& rGraphics,
                                const SlotEntry_t& rEntry,
                                const Rectangle_t& rRect) const
{
    const auto& style = Style().slotColumnPanel;
    rGraphics.DrawFilledRect(rRect.x, rRect.y, rRect.width, rRect.height, style.slotFillColor);
    rGraphics.DrawRect(rRect.x, rRect.y, rRect.width, rRect.height,
        style.slotBorderColor, Style().outline.width);

    const float padding = rRect.height * style.paddingRatio;
    const unsigned int labelSize =
        static_cast<unsigned int>(rRect.height * style.labelFontSizeRatio);
    const unsigned int nameSize =
        static_cast<unsigned int>(rRect.height * style.nameFontSizeRatio);

    const bool bRequired = rEntry.pSlotConfig->required;
    const std::string label = rEntry.pSlotConfig->displayName + (bRequired ? "" : " (opt)");
    rGraphics.DrawText(
        label, rRect.x + padding, rRect.y + padding, labelSize, style.labelTextColor);

    const UnitComponentConfig_t* pComp = rEntry.getComponent();
    const std::string& rName = pComp ? pComp->name : "(none)";
    const Color_t nameColor = pComp ? style.filledNameColor : style.emptyNameColor;
    const float nameY =
        rRect.y + padding + static_cast<float>(labelSize) * style.nameLabelSpacingMultiplier;
    rGraphics.DrawText(rName, rRect.x + padding, nameY, nameSize, nameColor);
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
