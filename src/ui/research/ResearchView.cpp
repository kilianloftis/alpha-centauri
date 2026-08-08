#include "ui/research/ResearchView.h"
#include "ui/research/CurrentResearchPanel.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

ResearchView::ResearchView(const ResearchManager& rResearch, WindowLayout_t layout)
    : IGameView(layout)
    , m_rResearch(rResearch)
{
    m_elements.push_back(std::make_unique<CurrentResearchPanel>(
        m_rResearch, ResolveLayout(m_layout, Style().layouts.topPanel)));
}

bool ResearchView::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
    return false;
}

} // namespace ac
