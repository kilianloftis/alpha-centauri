#include "ui/base/PopulationDisplay.h"
#include "graphics/Graphics.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/population/pop-types/Pop.h"
#include <sstream>

namespace ac
{

PopulationDisplay::PopulationDisplay(const Graphics& rGraphics, const PopContainer* pPopContainer, ResolvedLayout_t layout, PopButton::OnClickCallback_t onPopClick)
    : UIGroup(layout, rGraphics)
    , m_pPopulation(pPopContainer)
    , m_onPopClick(std::move(onPopClick))
{}

void PopulationDisplay::Update()
{
    if (!m_pPopulation)
    {
        throw std::runtime_error("PopulationDisplay: No population container set");
    }

    m_elements.clear();

    const float lineHeight = m_layout.height * kLineHeightRatio;

    float offsetY = lineHeight;
    for (const auto& pPop : m_pPopulation->GetPops())
    {
        const float buttonY = m_layout.y + offsetY;

        ResolvedLayout_t buttonLayout{
            m_layout.x,
            buttonY,
            m_layout.width,
            lineHeight
        };

        m_elements.push_back(std::make_unique<PopButton>(m_rGraphics, *pPop, buttonLayout, m_onPopClick));

        offsetY += lineHeight;
    }
}

void PopulationDisplay::Render()
{
    if (!m_pPopulation)
    {
        throw std::runtime_error("PopulationDisplay: No population container set");
    }

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * kHeaderFontSizeRatio);

    std::ostringstream oss;
    oss << "Population: " << m_pPopulation->GetSize();
    m_rGraphics.DrawText(oss.str(), m_layout.x, m_layout.y, headerFontSize);

    // Render all pop buttons (stored in m_elements from UIGroup)
    for (const auto& pElement : m_elements)
    {
        pElement->Render();
    }
}

void PopulationDisplay::HandleKey(const KeyEvent_t& rEvent)
{
    // TODO: Handle keyboard navigation if needed
}

} // namespace ac
