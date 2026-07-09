#include "ui/research/CurrentResearchPanel.h"
#include "graphics/Graphics.h"

namespace ac
{

namespace
{

constexpr RatioLayout_t k_CurrentResearchLabelLayout    {0.0f, 0.0f,  1.0f, 0.35f};
constexpr RatioLayout_t k_CurrentResearchTargetLayout   {0.0f, 0.35f, 1.0f, 0.4f};
constexpr RatioLayout_t k_CurrentResearchProgressLayout {0.0f, 0.75f, 1.0f, 0.25f};
constexpr Color k_BackgroundColor                       {30, 30, 50, 255};
constexpr Color k_BorderColor                           {80, 80, 120, 255};
constexpr unsigned int k_LabelFontSize                  = 14;
constexpr unsigned int k_TargetFontSize                 = 16;
constexpr unsigned int k_ProgressFontSize               = 13;

} // namespace

CurrentResearchPanel::CurrentResearchPanel(const ResearchManager* pResearch, WindowLayout_t layout)
    : UIElement(layout)
    , m_pResearch(pResearch)
{}

void CurrentResearchPanel::Render(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BackgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BorderColor);

    const WindowLayout_t labelArea    = ResolveLayout(m_layout, k_CurrentResearchLabelLayout);
    const WindowLayout_t targetArea   = ResolveLayout(m_layout, k_CurrentResearchTargetLayout);
    const WindowLayout_t progressArea = ResolveLayout(m_layout, k_CurrentResearchProgressLayout);

    rGraphics.DrawText("Current Research Target:", labelArea.x, labelArea.y, k_LabelFontSize, Color::White());

    if (m_pResearch && m_pResearch->HasResearchTarget())
    {
        rGraphics.DrawText(m_pResearch->GetResearchTarget(), targetArea.x, targetArea.y, k_TargetFontSize, Color::Yellow());

        const int accumulated = m_pResearch->GetAccumulatedPoints();
        const int needed      = m_pResearch->GetPointsNeededForCurrentTech();
        const std::string progressText = std::to_string(accumulated) + " / " + std::to_string(needed) + " RP";
        rGraphics.DrawText(progressText, progressArea.x, progressArea.y, k_ProgressFontSize, Color::Green());
    }
    else
    {
        rGraphics.DrawText("None", targetArea.x, targetArea.y, k_TargetFontSize, Color::Yellow());
    }
}

} // namespace ac
