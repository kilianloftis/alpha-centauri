#include "ui/research/ResearchView.h"
#include "ui/research/CurrentResearchPanel.h"
#include "graphics/Graphics.h"

namespace ac
{

ResearchView::ResearchView(ResearchManager* pResearch, WindowLayout_t layout)
    : UIGroup(layout)
    , m_pResearch(pResearch)
{
    m_elements.push_back(std::make_unique<CurrentResearchPanel>(pResearch, Resolve(layout, k_TopPanelLayout)));
}

void ResearchView::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
    }
}

} // namespace ac
