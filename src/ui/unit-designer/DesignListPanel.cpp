#include "ui/unit-designer/DesignListPanel.h"
#include "game/faction/Military.h"
#include "game/units/UnitDesign.h"
#include "graphics/Graphics.h"

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
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{10, 10, 15, 255});
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{60, 60, 80, 255});

    if (!m_pMilitary)
    {
        return;
    }

    const auto& rDesigns = m_pMilitary->GetDesigns();

    if (rDesigns.empty())
    {
        const unsigned int labelSize = static_cast<unsigned int>(m_layout.height * k_LabelFontRatio);
        const float padding = m_layout.height * k_BoxPaddingRatio * 4.0f;
        rGraphics.DrawText(
            "No designs saved — select components above and click Save Design",
            m_layout.x + padding,
            m_layout.y + padding,
            labelSize,
            Color{100, 100, 100, 255}
        );
        return;
    }

    const float boxPad    = m_layout.height * k_BoxPaddingRatio;
    const float boxWidth  = m_layout.width * k_BoxWidthRatio;
    const float boxHeight = m_layout.height - boxPad * 2.0f;
    const unsigned int fontSize = static_cast<unsigned int>(m_layout.height * k_FontSizeRatio);

    float x = m_layout.x + boxPad;
    for (const auto& pDesign : rDesigns)
    {
        const bool bSelected   = pDesign.get() == m_pSelectedDesign;
        const Color fillColor  = bSelected ? Color{50, 50, 90, 255} : Color{25, 25, 40, 255};
        const Color borderColor = bSelected ? Color::Yellow() : Color{80, 80, 110, 255};

        rGraphics.DrawFilledRect(x, m_layout.y + boxPad, boxWidth, boxHeight, fillColor);
        rGraphics.DrawRect(x, m_layout.y + boxPad, boxWidth, boxHeight, borderColor);

        const float textPad = boxWidth * 0.05f;
        rGraphics.DrawText(
            pDesign->GetName(),
            x + textPad,
            m_layout.y + boxPad + boxHeight * 0.35f,
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
    const float boxHeight = m_layout.height - boxPad * 2.0f;

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
