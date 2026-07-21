#include "ui/research/CurrentResearchPanel.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

CurrentResearchPanel::CurrentResearchPanel(const ResearchManager* pResearch, WindowLayout_t layout)
    : UIElement(layout)
    , m_pResearch(pResearch)
{}

void CurrentResearchPanel::Render(Graphics& rGraphics)
{
    const auto& style = Style().currentResearchPanel;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    const WindowLayout_t labelArea    = ResolveLayout(m_layout, style.labelLayout);
    const WindowLayout_t targetArea   = ResolveLayout(m_layout, style.targetLayout);
    const WindowLayout_t progressArea = ResolveLayout(m_layout, style.progressLayout);

    rGraphics.DrawText("Current Research Target:", labelArea.x, labelArea.y, style.labelFontSize, style.labelColor);

    if (m_pResearch && m_pResearch->HasResearchTarget())
    {
        rGraphics.DrawText(m_pResearch->GetResearchTarget(), targetArea.x, targetArea.y, style.targetFontSize, style.targetColor);

        const int accumulated = m_pResearch->GetAccumulatedPoints();
        const int needed      = m_pResearch->GetPointsNeededForCurrentTech();
        const std::string progressText = std::to_string(accumulated) + " / " + std::to_string(needed) + " RP";
        rGraphics.DrawText(progressText, progressArea.x, progressArea.y, style.progressFontSize, style.progressColor);
    }
    else
    {
        rGraphics.DrawText("None", targetArea.x, targetArea.y, style.targetFontSize, style.targetColor);
    }
}

} // namespace ac
