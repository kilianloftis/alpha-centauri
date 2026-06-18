#include "ui/base/PopulationDisplay.h"
#include "graphics/Graphics.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/population/pop-types/Pop.h"
#include <sstream>

namespace ac
{

PopulationDisplay::PopulationDisplay(const PopContainer* pPopContainer, WindowLayout_t layout, PopClickCallback_t onPopClick)
    : UIElement(layout)
    , m_pPopulation(pPopContainer)
    , m_onPopClick(std::move(onPopClick))
{}

void PopulationDisplay::Update()
{
}

void PopulationDisplay::Render(Graphics& rGraphics)
{
    if (!m_pPopulation)
    {
        throw std::runtime_error("PopulationDisplay: No population container set");
    }

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * k_HeaderFontSizeRatio);

    std::ostringstream oss;
    oss << "Population: " << m_pPopulation->GetSize();
    rGraphics.DrawText(oss.str(), m_layout.x, m_layout.y, headerFontSize);

    const float boxSize = m_layout.height * k_PopBoxSizeRatio;
    const float popFontSize = static_cast<unsigned int>(boxSize * k_PopBoxFontSizeRatio);
    const float startY = m_layout.y + headerFontSize + m_layout.height * k_PopRowYOffsetRatio;

    m_popBoxes.clear();
    const auto& pops = m_pPopulation->GetPops();

    const float totalBoxWidth = pops.size() * boxSize;
    float boxSpacing = m_layout.height * k_PopBoxSpacingRatio;
    if (pops.size() > 1 && totalBoxWidth > m_layout.width)
    {
        boxSpacing = (m_layout.width - totalBoxWidth) / (pops.size() - 1);
    }

    for (size_t i = 0; i < pops.size(); ++i)
    {
        const float boxX = m_layout.x + i * (boxSize + boxSpacing);
        const float boxY = startY;

        rGraphics.DrawFilledRect(boxX, boxY, boxSize, boxSize, Color::Blue());
        rGraphics.DrawRect(boxX, boxY, boxSize, boxSize, Color::White(), 2.0f);

        m_popBoxes.push_back(PopBox_t{{boxX, boxY, boxSize, boxSize}, pops[i].get()});

        const char* popType = pops[i]->GetPopType();
        char firstLetter = popType && popType[0] ? popType[0] : '?';
        std::string letterStr(1, firstLetter);

        const float textX = boxX + boxSize * k_PopBoxTextXOffsetRatio;
        const float textY = boxY + boxSize * k_PopBoxTextYOffsetRatio;
        rGraphics.DrawText(letterStr, textX, textY, popFontSize, Color::White());
    }
}

void PopulationDisplay::HandleMouse(const MouseEvent_t& rEvent)
{
    if (rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    for (auto it = m_popBoxes.rbegin(); it != m_popBoxes.rend(); ++it)
    {
        if (rEvent.x >= it->bounds.x && rEvent.x < it->bounds.x + it->bounds.width &&
            rEvent.y >= it->bounds.y && rEvent.y < it->bounds.y + it->bounds.height)
        {
            if (m_onPopClick)
            {
                m_onPopClick(it->pPop);
            }
            break;
        }
    }
}

} // namespace ac
