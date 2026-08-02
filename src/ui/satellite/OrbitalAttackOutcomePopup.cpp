#include "ui/satellite/OrbitalAttackOutcomePopup.h"

#include "ui/satellite/SatelliteLabeledButton.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "ui/style/UiStyle.h"

namespace ac
{

OrbitalAttackOutcomePopup::OrbitalAttackOutcomePopup(WindowLayout_t layout,
                                                     std::string message,
                                                     std::function<void()> onOk)
    : UIElement(layout)
    , m_message(std::move(message))
    , m_onOk(std::move(onOk))
{
    const auto& style = Style().satelliteView;
    m_pOkButton = std::make_unique<SatelliteLabeledButton>(
        ResolveLayout(m_layout, style.outcomeOkLayout),
        "OK",
        [this]() { Close_(); },
        /*bSelected*/ false);
}

void OrbitalAttackOutcomePopup::Close_()
{
    m_bShouldClose = true;
    if (m_onOk)
    {
        m_onOk();
    }
}

void OrbitalAttackOutcomePopup::Render(Graphics& rGraphics)
{
    if (m_bShouldClose)
    {
        return;
    }

    const auto& style = Style().satelliteView;
    const auto& popupStyle = Style().productionSelectorPopup;
    const float padding = popupStyle.paddingRatio * m_layout.width;
    const unsigned int headerFontSize =
        static_cast<unsigned int>(m_layout.height * popupStyle.headerFontSizeRatio);
    const unsigned int entryFontSize =
        static_cast<unsigned int>(m_layout.height * popupStyle.entryFontSizeRatio);

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    rGraphics.DrawText(
        "Orbital Attack",
        m_layout.x + padding,
        m_layout.y + padding,
        headerFontSize,
        style.headerColor);

    rGraphics.DrawText(
        m_message,
        m_layout.x + padding,
        m_layout.y + m_layout.height * popupStyle.headerLineOffset * popupStyle.lineHeightRatio,
        entryFontSize,
        style.cellColor);

    if (m_pOkButton)
    {
        m_pOkButton->Render(rGraphics);
    }
}

bool OrbitalAttackOutcomePopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape || rEvent.key == Key_t::Enter)
    {
        Close_();
        return true;
    }
    return false;
}

void OrbitalAttackOutcomePopup::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (m_bShouldClose || rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    if (m_pOkButton
        && m_pOkButton->Contains(static_cast<float>(rEvent.x), static_cast<float>(rEvent.y)))
    {
        m_pOkButton->HandleMouseClick(rEvent);
    }
}

} // namespace ac
