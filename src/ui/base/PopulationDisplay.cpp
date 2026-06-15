#include "ui/base/PopulationDisplay.h"
#include "graphics/Graphics.h"
#include "game/faction/base/population/PopContainer.h"
#include <sstream>

namespace ac
{

PopulationDisplay::PopulationDisplay(EventBus& rBus, Graphics& rGraphics)
    : m_rBus(rBus)
    , m_rGraphics(rGraphics)
    , m_currentPop(0)
{
    // Subscribe to population gained events
    m_subscriptionId = m_rBus.subscribe<EvBaseGainedPop>(
        [this](const EvBaseGainedPop& event) {
            OnPopGained_(event);
        }
    );
}

PopulationDisplay::~PopulationDisplay()
{
    m_rBus.unsubscribe(m_subscriptionId);
}

void PopulationDisplay::SetRenderPosition(float x, float y)
{
    m_renderX = x;
    m_renderY = y;
}

void PopulationDisplay::SetPopulation(const PopContainer* pPopContainer)
{
    m_pPopulation = pPopContainer;
}

void PopulationDisplay::Render(Graphics& /*rGraphics*/)
{
    Render(m_renderX, m_renderY);
}

void PopulationDisplay::Render(float x, float y)
{
    std::ostringstream oss;
    oss << "Population: " << m_currentPop;
    m_rGraphics.DrawText(oss.str(), x, y, 24);

    if (!m_pPopulation)
    {
        return;
    }

    const float lineHeight = 28.0f;
    float offsetY = lineHeight;
    for (const auto& pPop : m_pPopulation->GetPops())
    {
        m_rGraphics.DrawText(pPop->GetPopType(), x, y + offsetY, 20);
        offsetY += lineHeight;
    }
}

int PopulationDisplay::GetCurrentPop() const
{
    return m_currentPop;
}

void PopulationDisplay::SetCurrentPop(int pop)
{
    m_currentPop = pop;
}

void PopulationDisplay::OnPopGained_(const EvBaseGainedPop& event)
{
    m_currentPop = event.newSize;
}

} // namespace ac
