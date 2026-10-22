#include "ui/unit-designer/DesignListPanel.h"
#include "game/units/Military.h"
#include "game/units/IDesign.h"
#include "game/units/UnitDesign.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

DesignListPanel::DesignListPanel(
    const Military& rMilitary,
    WindowLayout_t layout,
    std::function<void(const UnitDesign*)> onDesignSelected
)
    : UIElement(layout)
    , m_rMilitary(rMilitary)
    , m_onDesignSelected(std::move(onDesignSelected))
{}

void DesignListPanel::Render(Graphics& rGraphics)
{
    const auto& style = Style().designListPanel;
    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    const std::vector<DesignBox_t> boxes = LayoutBoxes_();
    if (boxes.empty())
    {
        const unsigned int labelSize =
            static_cast<unsigned int>(m_layout.height * style.labelFontRatio);
        const float padding =
            m_layout.height * style.boxPaddingRatio * style.emptyListPaddingMultiplier;
        rGraphics.DrawText(
            "No designs saved — select components above and click Save Design",
            m_layout.x + padding,
            m_layout.y + padding,
            labelSize,
            style.emptyListTextColor
        );
        return;
    }

    const unsigned int fontSize = static_cast<unsigned int>(m_layout.height * style.fontSizeRatio);
    for (const DesignBox_t& rBox : boxes)
    {
        const Rectangle_t& rRect = rBox.rect;
        const bool bSelected = rBox.pDesign == m_pSelectedDesign;
        const Color_t fillColor =
            bSelected ? style.selectedBoxFillColor : style.unselectedBoxFillColor;
        const Color_t borderColor =
            bSelected ? style.selectedBoxBorderColor : style.unselectedBoxBorderColor;
        rGraphics.DrawFilledRect(rRect.x, rRect.y, rRect.width, rRect.height, fillColor);
        rGraphics.DrawRect(rRect.x, rRect.y, rRect.width, rRect.height, borderColor);

        const float textX = rRect.x + rRect.width * style.textPadRatio;
        const float textY = rRect.y + rRect.height * style.textVerticalRatio;
        rGraphics.DrawText(rBox.pDesign->GetName(), textX, textY, fontSize);
    }
}

void DesignListPanel::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    for (const DesignBox_t& rBox : LayoutBoxes_())
    {
        if (ContainsMouseCoord(rBox.rect, rEvent))
        {
            m_pSelectedDesign = rBox.pDesign;
            m_onDesignSelected(m_pSelectedDesign);
            return;
        }
    }
}

std::vector<DesignListPanel::DesignBox_t> DesignListPanel::LayoutBoxes_() const
{
    const auto& style = Style().designListPanel;
    const float boxPad = m_layout.height * style.boxPaddingRatio;
    const float boxWidth = m_layout.width * style.boxWidthRatio;
    const float boxHeight = m_layout.height - boxPad * style.verticalPaddingMultiplier;

    std::vector<DesignBox_t> boxes;
    float x = m_layout.x + boxPad;
    for (const auto& pDesign : m_rMilitary.GetDesigns())
    {
        // Native designs are not the player's to edit.
        const auto* pUnitDesign = dynamic_cast<const UnitDesign*>(pDesign.get());
        if (!pUnitDesign)
        {
            continue;
        }
        boxes.push_back({pUnitDesign, {x, m_layout.y + boxPad, boxWidth, boxHeight}});

        x += boxWidth + boxPad;
        if (x + boxWidth > m_layout.x + m_layout.width)
        {
            break;
        }
    }
    return boxes;
}

} // namespace ac
