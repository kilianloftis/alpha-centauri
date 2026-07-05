#include "ui/unit-designer/DesignListPanel.h"
#include "game/faction/Military.h"
#include "game/units/UnitDesign.h"
#include "graphics/Graphics.h"

namespace ac
{

namespace
{

constexpr Color k_BackgroundColor           {10, 10, 15, 255};
constexpr Color k_BorderColor               {60, 60, 80, 255};
constexpr Color k_EmptyListTextColor        {100, 100, 100, 255};
constexpr Color k_SelectedBoxFillColor      {50, 50, 90, 255};
constexpr Color k_UnselectedBoxFillColor    {25, 25, 40, 255};
constexpr Color k_UnselectedBoxBorderColor  {80, 80, 110, 255};
constexpr float k_BoxWidthRatio             = 0.15f;
constexpr float k_BoxPaddingRatio           = 0.005f;
constexpr float k_FontSizeRatio             = 0.07f;
constexpr float k_LabelFontRatio            = 0.05f;
constexpr float k_EmptyListPaddingMultiplier = 4.0f;
constexpr float k_VerticalPaddingMultiplier = 2.0f;
constexpr float k_TextPadRatio              = 0.05f;
constexpr float k_TextVerticalRatio         = 0.35f;

} // namespace

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
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BackgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BorderColor);

    if (!m_pMilitary)
    {
        return;
    }

    const auto& rDesigns = m_pMilitary->GetDesigns();

    if (rDesigns.empty())
    {
        const unsigned int labelSize = static_cast<unsigned int>(m_layout.height * k_LabelFontRatio);
        const float padding = m_layout.height * k_BoxPaddingRatio * k_EmptyListPaddingMultiplier;
        rGraphics.DrawText(
            "No designs saved — select components above and click Save Design",
            m_layout.x + padding,
            m_layout.y + padding,
            labelSize,
            k_EmptyListTextColor
        );
        return;
    }

    const float boxPad    = m_layout.height * k_BoxPaddingRatio;
    const float boxWidth  = m_layout.width * k_BoxWidthRatio;
    const float boxHeight = m_layout.height - boxPad * k_VerticalPaddingMultiplier;
    const unsigned int fontSize = static_cast<unsigned int>(m_layout.height * k_FontSizeRatio);

    float x = m_layout.x + boxPad;
    for (const auto& pDesign : rDesigns)
    {
        const bool bSelected   = pDesign.get() == m_pSelectedDesign;
        const Color fillColor  = bSelected ? k_SelectedBoxFillColor : k_UnselectedBoxFillColor;
        const Color borderColor = bSelected ? Color::Yellow() : k_UnselectedBoxBorderColor;

        rGraphics.DrawFilledRect(x, m_layout.y + boxPad, boxWidth, boxHeight, fillColor);
        rGraphics.DrawRect(x, m_layout.y + boxPad, boxWidth, boxHeight, borderColor);

        const float textPad = boxWidth * k_TextPadRatio;
        rGraphics.DrawText(
            pDesign->GetName(),
            x + textPad,
            m_layout.y + boxPad + boxHeight * k_TextVerticalRatio,
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

    const auto& rDesigns  = m_pMilitary->GetDesigns();
    const float boxPad    = m_layout.height * k_BoxPaddingRatio;
    const float boxWidth  = m_layout.width * k_BoxWidthRatio;
    const float boxHeight = m_layout.height - boxPad * k_VerticalPaddingMultiplier;

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
