#include "ui/research/CurrentResearchPanel.h"
#include "graphics/Graphics.h"

namespace ac
{

void CurrentResearchPanel::Draw(Graphics& rGraphics)
{
    // Draw panel background
    rGraphics.DrawFilledRect(m_x, m_y, m_width, m_height, Color{30, 30, 50});
    rGraphics.DrawRect(m_x, m_y, m_width, m_height, Color{80, 80, 120});

    // Draw current research target
    const float textX = m_x + 10.f;
    const float textY = m_y + 10.f;

    rGraphics.DrawText("Current Research Target:", textX, textY, 14, Color::White());

    if (m_pResearchView)
    {
        rGraphics.DrawText(m_pResearchView->GetCurrentResearchTarget(), textX, textY + 25.f, 16, Color::Yellow());
    }
    else
    {
        rGraphics.DrawText("None", textX, textY + 25.f, 16, Color::Yellow());
    }
}

void CurrentResearchPanel::Update(float /*deltaTime*/)
{
}

void CurrentResearchPanel::HandleMouse(const MouseEvent_t& /*rEvent*/)
{
}

} // namespace ac
