#include "ui/NoticePopup.h"

#include "ui/satellite/SatelliteLabeledButton.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "ui/style/UiStyle.h"

namespace ac
{

NoticePopup::NoticePopup(WindowLayout_t layout,
                         std::string title,
                         std::string message,
                         std::function<void()> onOk)
    : UIElement(layout)
    , m_title(std::move(title))
    , m_message(std::move(message))
    , m_onOk(std::move(onOk))
{
    const NoticePopupStyle_t& rStyle = Style().noticePopup;
    m_pOkButton = std::make_unique<SatelliteLabeledButton>(
        ResolveLayout(m_layout, rStyle.okButtonLayout),
        "OK",
        [this]() { Close_(); },
        /*bSelected*/ false);
}

NoticePopup::~NoticePopup() = default;

void NoticePopup::Close_()
{
    m_bShouldClose = true;
    if (m_onOk)
    {
        m_onOk();
    }
}

void NoticePopup::Render(Graphics& rGraphics)
{
    if (m_bShouldClose)
    {
        return;
    }

    const NoticePopupStyle_t& rStyle = Style().noticePopup;
    const ListSelectorPopupStyle_t& rPopupStyle = Style().listSelectorPopup;
    const float padding = rPopupStyle.paddingRatio * m_layout.width;
    const unsigned int headerFontSize =
        static_cast<unsigned int>(m_layout.height * rPopupStyle.headerFontSizeRatio);
    const unsigned int entryFontSize =
        static_cast<unsigned int>(m_layout.height * rPopupStyle.entryFontSizeRatio);

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, rStyle.backgroundColor);
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, rStyle.borderColor);

    rGraphics.DrawText(
        m_title,
        m_layout.x + padding,
        m_layout.y + padding,
        headerFontSize,
        rStyle.headerColor);

    rGraphics.DrawText(
        m_message,
        m_layout.x + padding,
        m_layout.y + m_layout.height * rPopupStyle.headerLineOffset * rPopupStyle.lineHeightRatio,
        entryFontSize,
        rStyle.messageColor);

    if (m_pOkButton)
    {
        m_pOkButton->Render(rGraphics);
    }
}

bool NoticePopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape || rEvent.key == Key_t::Enter)
    {
        Close_();
        return true;
    }
    return false;
}

void NoticePopup::HandleMouseClick(const MouseEvent_t& rEvent)
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
