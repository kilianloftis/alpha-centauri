#include "ui/research/ResearchDisplay.h"
#include "ui/research/ResearchView.h"
#include "ui/research/CurrentResearchPanel.h"
#include "graphics/Graphics.h"

namespace ac
{

void ResearchDisplay::SetResearchView(const ResearchView* pResearchView)
{
    m_pResearchView = pResearchView;
    UpdatePanelsResearchView_();
}

void ResearchDisplay::Draw(Graphics& rGraphics)
{
    // Draw main panel background
    rGraphics.DrawFilledRect(m_x, m_y, m_width, m_height, Color{20, 20, 40});
    rGraphics.DrawRect(m_x, m_y, m_width, m_height, Color{100, 100, 160});

    // Draw title
    rGraphics.DrawText("Research", m_x + 10.f, m_y + 5.f, 16, Color::White());

    // Render all panels
    for (auto& pPanel : m_panels)
    {
        if (pPanel)
        {
            pPanel->Draw(rGraphics);
        }
    }

    // Draw instructions
    rGraphics.DrawText("Press ESC to close", m_x + 20.f, m_y + m_height - 30.f, 14, Color{150, 150, 150, 255});
}

void ResearchDisplay::Update(float deltaTime)
{
    for (auto& pPanel : m_panels)
    {
        if (pPanel)
        {
            pPanel->Update(deltaTime);
        }
    }
}

void ResearchDisplay::HandleMouse(const MouseEvent_t& rEvent)
{
    for (auto& pPanel : m_panels)
    {
        if (pPanel)
        {
            pPanel->HandleMouse(rEvent);
        }
    }
}

void ResearchDisplay::InitializePanels_()
{
    // Add the current research target panel
    auto pCurrentResearchPanel = std::make_unique<CurrentResearchPanel>();
    m_panels.push_back(std::move(pCurrentResearchPanel));

    // TODO: Add more panels as they are implemented
}

void ResearchDisplay::UpdatePanelsResearchView_()
{
    for (auto& pPanel : m_panels)
    {
        if (pPanel)
        {
            // Dynamic cast to CurrentResearchPanel to set the view
            if (auto pCurrentResearchPanel = dynamic_cast<CurrentResearchPanel*>(pPanel.get()))
            {
                pCurrentResearchPanel->SetResearchView(m_pResearchView);
            }
        }
    }
}

} // namespace ac
