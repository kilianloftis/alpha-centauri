#include "ui/unit-designer/DesignListPanel.h"
#include "game/faction/Military.h"
#include "game/units/UnitDesign.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

DesignListPanel::DesignListPanel(
    const Military* pMilitary,
    WindowLayout_t layout,
    std::function<void(const UnitDesign*)> onDesignSelected
)
    : UIElement(layout)
    , m_pMilitary(pMilitary)
    , m_onDesignSelected(std::move(onDesignSelected))
{}

void DesignListPanel::Render(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        Style().designListPanel.backgroundColor
    );
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        Style().designListPanel.borderColor
    );

    if (!m_pMilitary)
    {
        return;
    }

    const auto& rDesigns = m_pMilitary->GetDesigns();

    if (rDesigns.empty())
    {
        const unsigned int labelSize = static_cast<unsigned int>(
            m_layout.height * Style().designListPanel.labelFontRatio);
        const float padding = m_layout.height * Style().designListPanel.boxPaddingRatio
            * Style().designListPanel.emptyListPaddingMultiplier;
        rGraphics.DrawText(
            "No designs saved — select components above and click Save Design",
            m_layout.x + padding,
            m_layout.y + padding,
            labelSize,
            Style().designListPanel.emptyListTextColor
        );
        return;
    }

    const float boxPad = m_layout.height * Style().designListPanel.boxPaddingRatio;
    const float boxWidth = m_layout.width * Style().designListPanel.boxWidthRatio;
    const float boxHeight =
        m_layout.height - boxPad * Style().designListPanel.verticalPaddingMultiplier;
    const unsigned int fontSize = static_cast<unsigned int>(
        m_layout.height * Style().designListPanel.fontSizeRatio);

    float x = m_layout.x + boxPad;
    for (const auto& pDesign : rDesigns)
    {
        const bool bSelected = pDesign.get() == m_pSelectedDesign;
        const Color_t fillColor = bSelected
            ? Style().designListPanel.selectedBoxFillColor
            : Style().designListPanel.unselectedBoxFillColor;
        const Color_t borderColor = bSelected
            ? Style().designListPanel.selectedBoxBorderColor
            : Style().designListPanel.unselectedBoxBorderColor;

        rGraphics.DrawFilledRect(x, m_layout.y + boxPad, boxWidth, boxHeight, fillColor);
        rGraphics.DrawRect(x, m_layout.y + boxPad, boxWidth, boxHeight, borderColor);

        const float textPad = boxWidth * Style().designListPanel.textPadRatio;
        rGraphics.DrawText(
            pDesign->GetName(),
            x + textPad,
            m_layout.y + boxPad + boxHeight * Style().designListPanel.textVerticalRatio,
            fontSize
        );

        x += boxWidth + boxPad;
        if (x + boxWidth > m_layout.x + m_layout.width)
        {
            break;
        }
    }
}

void DesignListPanel::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button != MouseButton_t::Left || !m_pMilitary)
    {
        return;
    }

    const auto& rDesigns = m_pMilitary->GetDesigns();
    const float boxPad = m_layout.height * Style().designListPanel.boxPaddingRatio;
    const float boxWidth = m_layout.width * Style().designListPanel.boxWidthRatio;
    const float boxHeight =
        m_layout.height - boxPad * Style().designListPanel.verticalPaddingMultiplier;

    float x = m_layout.x + boxPad;
    for (const auto& pDesign : rDesigns)
    {
        const Rectangle_t rect{x, m_layout.y + boxPad, boxWidth, boxHeight};
        if (ContainsMouseCoord(rect, rEvent))
        {
            m_pSelectedDesign = pDesign.get();
            if (m_onDesignSelected)
            {
                m_onDesignSelected(m_pSelectedDesign);
            }
            return;
        }
        x += boxWidth + boxPad;
    }
}

} // namespace ac
