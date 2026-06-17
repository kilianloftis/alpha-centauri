#include "ui/base/PopulationDisplay.h"
#include "graphics/Graphics.h"
#include "game/faction/base/population/PopContainer.h"
#include <sstream>

namespace ac
{

static constexpr float k_headerFontSizeRatio = 0.04f;
static constexpr float k_entryFontSizeRatio  = 0.03f;
static constexpr float k_lineHeightRatio     = 0.05f;

PopulationDisplay::PopulationDisplay(const Graphics& rGraphics, const PopContainer* pPopContainer, PanelLayout_t layout)
    : UIPanel(layout)
    , m_rGraphics(rGraphics)
    , m_pPopulation(pPopContainer)
{
    // Subscribe to population gained events
}

void PopulationDisplay::Render()
{
    if (!m_pPopulation)
    {
        throw std::runtime_error("PopulationDisplay: No population container set");
    }

    const auto [x, y, width, height] = m_layout.Resolve(
        static_cast<float>(rGraphics.GetWindowWidth()),
        static_cast<float>(rGraphics.GetWindowHeight())
    );

    const unsigned int headerFontSize = static_cast<unsigned int>(height * k_headerFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(height * k_entryFontSizeRatio);
    const float        lineHeight     = height * k_lineHeightRatio;

    std::ostringstream oss;
    oss << "Population: " << m_pPopulation->GetSize();
    m_rGraphics.DrawText(oss.str(), x, y, headerFontSize);

    float offsetY = lineHeight;
    for (const auto& pPop : m_pPopulation->GetPops())
    {
        m_rGraphics.DrawText(pPop->GetPopType(), x, y + offsetY, entryFontSize);
        offsetY += lineHeight;
    }
}

} // namespace ac
