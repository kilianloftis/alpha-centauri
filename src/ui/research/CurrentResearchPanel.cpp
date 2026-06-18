#include "ui/research/CurrentResearchPanel.h"
#include "graphics/Graphics.h"

namespace ac
{

CurrentResearchPanel::CurrentResearchPanel(ResearchManager* pResearch, WindowLayout_t layout)
    : UIElement(layout)
    , m_pResearch(pResearch)
{}

void CurrentResearchPanel::Render(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{30, 30, 50});
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{80, 80, 120});

    const WindowLayout_t labelArea    = Resolve(m_layout, k_CurrentResearchLabelLayout);
    const WindowLayout_t targetArea   = Resolve(m_layout, k_CurrentResearchTargetLayout);
    const WindowLayout_t progressArea = Resolve(m_layout, k_CurrentResearchProgressLayout);

    rGraphics.DrawText("Current Research Target:", labelArea.x, labelArea.y, 14, Color::White());

    if (m_pResearch && m_pResearch->HasResearchTarget())
    {
        rGraphics.DrawText(m_pResearch->GetResearchTarget(), targetArea.x, targetArea.y, 16, Color::Yellow());

        const int accumulated = m_pResearch->GetAccumulatedPoints();
        const int needed      = m_pResearch->GetPointsNeededForCurrentTech();
        const std::string progressText = std::to_string(accumulated) + " / " + std::to_string(needed) + " RP";
        rGraphics.DrawText(progressText, progressArea.x, progressArea.y, 13, Color::Green());
    }
    else
    {
        rGraphics.DrawText("None", targetArea.x, targetArea.y, 16, Color::Yellow());
    }
}

void CurrentResearchPanel::Update()
{
}

} // namespace ac
