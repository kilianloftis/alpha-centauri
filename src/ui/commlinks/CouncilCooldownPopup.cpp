#include "ui/commlinks/CouncilCooldownPopup.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "ui/style/UiStyle.h"

#include <string>
#include <vector>

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
    m_onOk();
}

void CouncilCooldownPopup::Render(Graphics& rGraphics)
{
    if (m_bShouldClose)
    {
        return;
    }

    const auto& style = Style().listSelectorPopup;
    const float padding = style.paddingRatio * m_layout.width;
    const unsigned int headerFontSize =
        static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);
    const unsigned int entryFontSize =
        static_cast<unsigned int>(m_layout.height * style.entryFontSizeRatio);
    const float lineHeight = m_layout.height * style.lineHeightRatio;
    const float textX = m_layout.x + padding;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
                             style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        style.borderColor, Style().outline.width);
    rGraphics.DrawText(
        "Planetary Council", textX, m_layout.y + padding, headerFontSize, style.headerColor);

    float textY = m_layout.y + lineHeight * style.headerLineOffset;
    rGraphics.DrawText("You may not call another council meeting yet.", textX, textY,
                       entryFontSize, style.hintColor);
    for (const std::string& rLine : DetailLines_())
    {
        textY += lineHeight;
        rGraphics.DrawText(rLine, textX, textY, entryFontSize, style.entryColor);
    }

    DrawOkButton_(rGraphics);
}

std::vector<std::string> CouncilCooldownPopup::DetailLines_() const
{
    const std::string lastProposal = m_lastProposedYear
        ? "Mission Year " + std::to_string(*m_lastProposedYear)
        : "none";
    return {
        "Member cooldown: " + std::to_string(m_memberCooldownYears) + " years",
        "Governor cooldown: " + std::to_string(m_governorCooldownYears) + " years",
        // Which of the two intervals above applies to this player.
        "Your cooldown: " + std::to_string(m_playerCooldownYears) + " years",
        "Last proposal: " + lastProposal,
        "Years remaining: " + std::to_string(m_yearsRemaining),
    };
}

void CouncilCooldownPopup::DrawOkButton_(Graphics& rGraphics) const
{
    const auto& buttonStyle = Style().commlinksButton;
    const Rectangle_t& rRect = m_okButtonRect;
    rGraphics.DrawFilledRect(rRect.x, rRect.y, rRect.width, rRect.height, buttonStyle.fillColor);
    rGraphics.DrawRect(rRect.x, rRect.y, rRect.width, rRect.height,
        buttonStyle.borderColor, Style().outline.width);
    rGraphics.DrawText("OK", rRect.x + buttonStyle.textPadX, rRect.y + buttonStyle.textPadY,
                       buttonStyle.fontSize, buttonStyle.labelColor);
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
