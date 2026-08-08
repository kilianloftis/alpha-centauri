#include "ui/social-engineering/SocialEngineeringView.h"

#include "ui/social-engineering/SocialEngineeringDisplay.h"
#include "ui/social-engineering/SocialEngineeringBottomPanel.h"
#include "ui/style/UiStyle.h"

namespace ac
{

SocialEngineeringView::SocialEngineeringView(
    Faction& rFaction,
    const SocialPolicyRegistry& rPolicyRegistry,
    const SocialRatingRegistry& rRatingRegistry,
    WindowLayout_t layout
)
    : IGameView(layout)
{
    m_elements.push_back(std::make_unique<SocialEngineeringDisplay>(
        rFaction,
        rPolicyRegistry,
        rRatingRegistry,
        ResolveLayout(m_layout, Style().layouts.topPanel)
    ));
    m_elements.push_back(std::make_unique<SocialEngineeringBottomPanel>(
        rFaction,
        ResolveLayout(m_layout, Style().layouts.bottomPanel)
    ));
}

bool SocialEngineeringView::HandleKey(const KeyEvent_t& rEvent)
{
    if (IGameView::HandleKey(rEvent))
    {
        return true;
    }

    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }

    return false;
}

} // namespace ac
