#include "ui/base/BuildQueueDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/HurryProductionCalculator.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

BuildQueueDisplay::BuildQueueDisplay(
    const BaseManager& rBase,
    WindowLayout_t layout,
    std::function<void()> onHurryClicked
)
    : UIElement(layout)
    , m_onHurryClicked(std::move(onHurryClicked))
    , m_rBase(rBase)
{}

WindowLayout_t BuildQueueDisplay::HurryButtonLayout_() const
{
    return ResolveLayout(m_layout, Style().buildQueueDisplay.hurryButtonLayout);
}

bool BuildQueueDisplay::HurryEnabled_() const
{
    if (!m_onHurryClicked)
    {
        return false;
    }
    const HurryQuote_t quote = m_rBase.QuoteHurry();
    return quote.bAvailable && quote.creditCost > 0;
}

void BuildQueueDisplay::Render(Graphics& rGraphics)
{
    const auto& style = Style().buildQueueDisplay;

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);

    const unsigned int headerFontSize =
        static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);
    const float leftPadding = m_layout.width * style.leftPaddingRatio;
    rGraphics.DrawText(
        "Build Queue", m_layout.x + leftPadding, m_layout.y, headerFontSize, style.textColor);

    const WindowLayout_t button = HurryButtonLayout_();
    const bool bEnabled = HurryEnabled_();
    rGraphics.DrawFilledRect(
        button.x, button.y, button.width, button.height,
        bEnabled ? style.hurryFillColor : style.hurryDisabledFillColor);
    rGraphics.DrawRect(
        button.x, button.y, button.width, button.height, style.hurryBorderColor);

    const unsigned int hurryFontSize =
        static_cast<unsigned int>(m_layout.height * style.hurryFontSizeRatio);
    rGraphics.DrawText(
        "Hurry",
        button.x + button.width * style.hurryTextPadXRatio,
        button.y + button.height * style.hurryTextPadYRatio,
        hurryFontSize,
        bEnabled ? style.hurryLabelColor : style.hurryDisabledLabelColor);
}

void BuildQueueDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button != MouseButton_t::Left || !HurryEnabled_())
    {
        return;
    }
    if (ContainsMouseCoord(HurryButtonLayout_(), rEvent))
    {
        m_onHurryClicked();
    }
}

} // namespace ac
