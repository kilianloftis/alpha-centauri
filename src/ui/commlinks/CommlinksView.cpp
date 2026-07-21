#include "ui/commlinks/CommlinksView.h"
#include "ui/commlinks/CommlinksPanel.h"
#include "game/GameState.h"

namespace ac
{

CommlinksView::CommlinksView(GameState& rGameState, WindowLayout_t layout)
    : IGameView(layout)
    , m_rGameState(rGameState)
{
    m_elements.push_back(std::make_unique<CommlinksPanel>(
        m_rGameState,
        ResolveLayout(m_layout, k_PopupLayoutSmall)));
}

bool CommlinksView::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
    return false;
}

} // namespace ac
