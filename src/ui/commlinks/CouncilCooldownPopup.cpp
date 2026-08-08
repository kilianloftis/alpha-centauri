#include "ui/commlinks/CouncilCooldownPopup.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "ui/style/UiStyle.h"

#include <string>

namespace ac
{

CouncilCooldownPopup::CouncilCooldownPopup(
    WindowLayout_t layout,
    int memberCooldownYears,
    int governorCooldownYears,
    int playerCooldownYears,
    std::optional<int> lastProposedYear,
    int yearsRemaining,
    std::function<void()> onOk
)
    : UIElement(layout)
    , m_memberCooldownYears(memberCooldownYears)
    , m_governorCooldownYears(governorCooldownYears)
    , m_playerCooldownYears(playerCooldownYears)
    , m_lastProposedYear(lastProposedYear)
    , m_yearsRemaining(yearsRemaining)
    , m_onOk(std::move(onOk))
{
    CacheOkButtonRect_();
}

void CouncilCooldownPopup::CacheOkButtonRect_()
{
    const auto& style = Style().listSelectorPopup;
    const float padding = style.paddingRatio * m_layout.width;
    const float lineHeight = m_layout.height * style.lineHeightRatio;
    const float buttonWidth = m_layout.width * 0.35f;
    const float buttonHeight = lineHeight * 1.4f;
    m_okButtonRect = Rectangle_t{
        m_layout.x + (m_layout.width - buttonWidth) * 0.5f,
        m_layout.y + m_layout.height - padding - buttonHeight,
        buttonWidth,
        buttonHeight
    };
}

void CouncilCooldownPopup::Close_()
{
    m_bShouldClose = true;
    if (m_onOk)
    {
        m_onOk();
    }
}

void CouncilCooldownPopup::Render(Graphics& rGraphics)
{
    if (m_bShouldClose)
    {
        return;
    }

    const auto& style = Style().listSelectorPopup;
    const auto& buttonStyle = Style().commlinksButton;
    const float padding = style.paddingRatio * m_layout.width;
    const unsigned int headerFontSize =
        static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);
    const unsigned int entryFontSize =
        static_cast<unsigned int>(m_layout.height * style.entryFontSizeRatio);
    const float lineHeight = m_layout.height * style.lineHeightRatio;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
                             style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    float textY = m_layout.y + padding;
    rGraphics.DrawText(
        "Planetary Council",
        m_layout.x + padding,
        textY,
        headerFontSize,
        style.headerColor);

    textY = m_layout.y + lineHeight * style.headerLineOffset;
    rGraphics.DrawText(
        "You may not call another council meeting yet.",
        m_layout.x + padding,
        textY,
        entryFontSize,
        style.hintColor);
    textY += lineHeight;
    rGraphics.DrawText(
        "Member cooldown: " + std::to_string(m_memberCooldownYears) + " years",
        m_layout.x + padding,
        textY,
        entryFontSize,
        style.entryColor);
    textY += lineHeight;
    rGraphics.DrawText(
        "Governor cooldown: " + std::to_string(m_governorCooldownYears) + " years",
        m_layout.x + padding,
        textY,
        entryFontSize,
        style.entryColor);
    textY += lineHeight;
    // The one that actually applies to this player: which of the two intervals above they are
    // subject to. It was passed in and then discarded, so the popup showed both generic
    // intervals and left the player to guess which was theirs.
    rGraphics.DrawText(
        "Your cooldown: " + std::to_string(m_playerCooldownYears) + " years",
        m_layout.x + padding,
        textY,
        entryFontSize,
        style.entryColor);
    textY += lineHeight;

    const std::string lastProposalText = m_lastProposedYear
        ? ("Last proposal: Mission Year " + std::to_string(*m_lastProposedYear))
        : "Last proposal: none";
    rGraphics.DrawText(
        lastProposalText,
        m_layout.x + padding,
        textY,
        entryFontSize,
        style.entryColor);
    textY += lineHeight;
    rGraphics.DrawText(
        "Years remaining: " + std::to_string(m_yearsRemaining),
        m_layout.x + padding,
        textY,
        entryFontSize,
        style.entryColor);

    rGraphics.DrawFilledRect(
        m_okButtonRect.x, m_okButtonRect.y, m_okButtonRect.width, m_okButtonRect.height,
        buttonStyle.fillColor);
    rGraphics.DrawRect(
        m_okButtonRect.x, m_okButtonRect.y, m_okButtonRect.width, m_okButtonRect.height,
        buttonStyle.borderColor);
    rGraphics.DrawText(
        "OK",
        m_okButtonRect.x + buttonStyle.textPadX,
        m_okButtonRect.y + buttonStyle.textPadY,
        buttonStyle.fontSize,
        buttonStyle.labelColor);
}

bool CouncilCooldownPopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape || rEvent.key == Key_t::Enter)
    {
        Close_();
        return true;
    }
    return false;
}

void CouncilCooldownPopup::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (m_bShouldClose || rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    if (ContainsMouseCoord(m_okButtonRect, rEvent))
    {
        Close_();
    }
}

} // namespace ac
