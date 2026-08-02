#include "ui/council/CouncilVoteButton.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

CouncilVoteButton::CouncilVoteButton(WindowLayout_t layout, std::function<void()> onVote)
    : UIElement(layout)
    , m_onVote(std::move(onVote))
{
}

void CouncilVoteButton::Render(Graphics& rGraphics)
{
    const auto& s = Style().commlinksButton;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.fillColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.borderColor);
    rGraphics.DrawText(
        "Vote",
        m_layout.x + s.textPadX,
        m_layout.y + s.textPadY,
        s.fontSize,
        s.labelColor);
}

void CouncilVoteButton::HandleMouseClick(const MouseEvent_t& /*rEvent*/)
{
    if (m_onVote)
    {
        m_onVote();
    }
}

} // namespace ac
