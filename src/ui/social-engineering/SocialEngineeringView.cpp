#include "ui/social-engineering/SocialEngineeringView.h"

#include "ui/social-engineering/SocialEngineeringDisplay.h"
#include "ui/social-engineering/SocialEngineeringBottomPanel.h"

namespace ac
{

SocialEngineeringView::SocialEngineeringView(
    Faction* pFaction,
    const SocialPolicyRegistry* pPolicyRegistry,
    const SocialRatingRegistry* pRatingRegistry,
    WindowLayout_t layout
)
    : IGameView(layout)
    , m_pFaction(pFaction)
    , m_pPolicyRegistry(pPolicyRegistry)
    , m_pRatingRegistry(pRatingRegistry)
{
    m_elements.push_back(std::make_unique<SocialEngineeringDisplay>(
        m_pFaction,
        m_pPolicyRegistry,
        m_pRatingRegistry,
        ResolveLayout(m_layout, k_TopPanelLayout)
    ));
    m_elements.push_back(std::make_unique<SocialEngineeringBottomPanel>(
        m_pFaction,
        ResolveLayout(m_layout, k_BottomPanelLayout)
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
