#include "ui/research/ResearchView.h"
#include "ui/research/CurrentResearchPanel.h"
#include "game/research/TechRegistry.h"
#include "graphics/Graphics.h"
#include "ui/SpriteLibrary.h"
#include "ui/style/UiStyle.h"

namespace ac
{

ResearchView::ResearchView(const ResearchManager& rResearch,
                           const TechRegistry& rTechs,
                           SpriteLibrary& rSprites,
                           WindowLayout_t layout)
    : IGameView(layout)
    , m_rResearch(rResearch)
{
    m_elements.push_back(std::make_unique<CurrentResearchPanel>(
        m_rResearch, rTechs, rSprites, ResolveLayout(m_layout, Style().layouts.topPanel)));
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
