#include "ui/research/CurrentResearchPanel.h"
#include "game/research/TechConfigParser.h"
#include "game/research/TechRegistry.h"
#include "graphics/Graphics.h"
#include "ui/SpriteLibrary.h"
#include "ui/style/DrawPanelChrome.h"
#include "ui/style/UiStyle.h"

#include <algorithm>

namespace ac
{

CurrentResearchPanel::CurrentResearchPanel(const ResearchManager& rResearch,
                                           const TechRegistry& rTechs,
                                           SpriteLibrary& rSprites,
                                           WindowLayout_t layout)
    : UIElement(layout)
    , m_rResearch(rResearch)
    , m_rTechs(rTechs)
    , m_rSprites(rSprites)
{}

void CurrentResearchPanel::Render(Graphics& rGraphics)
{
    const auto& style = Style().currentResearchPanel;

    DrawPanelChrome(rGraphics, &m_rSprites, m_layout, style.backgroundColor,
                    style.backgroundSprite, style.borderColor);

    const WindowLayout_t labelArea    = ResolveLayout(m_layout, style.labelLayout);
    const WindowLayout_t targetArea   = ResolveLayout(m_layout, style.targetLayout);
    const WindowLayout_t progressArea = ResolveLayout(m_layout, style.progressLayout);
    const WindowLayout_t iconArea     = ResolveLayout(m_layout, style.iconLayout);

    rGraphics.DrawText("Current Research Target:", labelArea.x, labelArea.y, style.labelFontSize, style.labelColor);

    if (m_rResearch.HasResearchTarget())
    {
        const TechConfig_t* pTech = m_rTechs.Find(m_rResearch.GetResearchTarget());
        if (pTech && !pTech->icon.empty() && m_rSprites.Ensure(pTech->icon))
        {
            const float size = std::min(iconArea.width, iconArea.height);
            rGraphics.DrawSprite(pTech->icon, iconArea.x, iconArea.y, size, size);
        }

        rGraphics.DrawText(m_rResearch.GetResearchTargetName(), targetArea.x, targetArea.y,
                           style.targetFontSize, style.targetColor);

        const int accumulated = m_rResearch.GetAccumulatedPoints();
        const int needed      = m_rResearch.GetPointsNeededForCurrentTech();
        const std::string progressText = std::to_string(accumulated) + " / " + std::to_string(needed) + " RP";
        rGraphics.DrawText(progressText, progressArea.x, progressArea.y, style.progressFontSize, style.progressColor);
    }
    else
    {
        rGraphics.DrawText("None", targetArea.x, targetArea.y, style.targetFontSize, style.targetColor);
    }
}

} // namespace ac
