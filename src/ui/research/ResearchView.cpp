#include "ui/research/ResearchView.h"
#include "ui/UIManager.h"
#include "graphics/Graphics.h"

namespace ac
{

ResearchView::ResearchView(UIManager& rUIManager)
: m_rUIManager(rUIManager)
, m_pDisplay(std::make_unique<ResearchDisplay>())
, m_currentResearchTarget("None")
{
}

void ResearchView::Render()
{
    m_pDisplay->SetResearchView(this);
    m_pDisplay->Draw();
}

void ResearchView::Update(float deltaTime)
{
    m_pDisplay->Update(deltaTime);
}

void ResearchView::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_rUIManager.PopView();
    }
}

void ResearchView::HandleMouse(const MouseEvent_t& rEvent)
{
    m_pDisplay->HandleMouse(rEvent);
}

void ResearchView::OnPushed()
{
    // Initialize research target when view is opened
    // TODO: Get actual research target from game state
    m_currentResearchTarget = "Impact Rifles";
    m_pDisplay->SetResearchView(this);
}

void ResearchView::OnPopped()
{
    // Cleanup when view is closed
}

} // namespace ac
