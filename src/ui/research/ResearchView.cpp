#include "ui/research/ResearchView.h"
#include "ui/research/CurrentResearchPanel.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

ResearchView::ResearchView(const ResearchManager* pResearch, WindowLayout_t layout)
    : IGameView(layout)
    , m_pResearch(pResearch)
{
    m_elements.push_back(std::make_unique<CurrentResearchPanel>(pResearch, ResolveLayout(m_layout, Style().layouts.topPanel)));
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
