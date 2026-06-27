#include "ui/base/PopulationDisplay.h"
#include "graphics/Graphics.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/population/pop-types/Pop.h"
#include <algorithm>
#include <sstream>

namespace ac
{

namespace
{

// Returns sort group: 0=drones, 1=workers, 2=golden-age contributors, 3=specialists
int PopGroupOrder(const Pop& rPop)
{
    if (rPop.IsDrone())
        return 0;
    if (rPop.IsWorker() && rPop.GetGoldenAgeContribution() == 0)
        return 1;
    if (rPop.GetGoldenAgeContribution() > 0)
        return 2;
    return 3;
}

int SpecialistTotalOutput(const Pop& rPop)
{
    const SpecialistOutput_t out = rPop.GetSpecialistOutput();
    return out.econ + out.labs + out.psych;
}

} // namespace

PopulationDisplay::PopulationDisplay(const PopContainer* pPopContainer, WindowLayout_t layout, PopClickCallback_t onPopClick)
    : UIElement(layout)
    , m_pPopulation(pPopContainer)
    , m_onPopClick(std::move(onPopClick))
{}

void PopulationDisplay::Render(Graphics& rGraphics)
{
    if (!m_pPopulation)
    {
        throw std::runtime_error("PopulationDisplay: No population container set");
    }

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{20, 20, 20, 255});

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * k_HeaderFontSizeRatio);

    std::ostringstream oss;
    oss << "Population: " << m_pPopulation->GetSize();
    const float leftPadding = m_layout.width * k_LeftPaddingRatio;
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y, headerFontSize);

    const float boxSize = m_layout.height * k_PopBoxSizeRatio;
    const float popFontSize = static_cast<unsigned int>(boxSize * k_PopBoxFontSizeRatio);
    const float startY = m_layout.y + headerFontSize + m_layout.height * k_PopRowYOffsetRatio;

    m_popBoxes.clear();
    const auto& pops = m_pPopulation->GetPops();

    std::vector<Pop*> sortedPops;
    sortedPops.reserve(pops.size());
    for (const auto& pPop : pops)
        sortedPops.push_back(pPop.get());

    std::stable_sort(sortedPops.begin(), sortedPops.end(),
        [](const Pop* pA, const Pop* pB)
        {
            const int groupA = PopGroupOrder(*pA);
            const int groupB = PopGroupOrder(*pB);
            if (groupA != groupB)
                return groupA < groupB;
            if (groupA == 3)
            {
                const int outA = SpecialistTotalOutput(*pA);
                const int outB = SpecialistTotalOutput(*pB);
                if (outA != outB)
                    return outA > outB;
            }
            return std::string_view(pA->GetPopType()) < std::string_view(pB->GetPopType());
        });

    const float totalBoxWidth = sortedPops.size() * boxSize;
    float boxSpacing = m_layout.height * k_PopBoxSpacingRatio;
    if (sortedPops.size() > 1 && totalBoxWidth > m_layout.width)
    {
        boxSpacing = (m_layout.width - totalBoxWidth) / (sortedPops.size() - 1);
    }

    for (size_t i = 0; i < sortedPops.size(); ++i)
    {
        const float boxX = m_layout.x + leftPadding + i * (boxSize + boxSpacing);
        const float boxY = startY;

        rGraphics.DrawFilledRect(boxX, boxY, boxSize, boxSize, Color::Blue());
        rGraphics.DrawRect(boxX, boxY, boxSize, boxSize, Color::White(), 2.0f);

        m_popBoxes.push_back(PopBox_t{{boxX, boxY, boxSize, boxSize}, sortedPops[i]});

        const char* popType = sortedPops[i]->GetPopType();
        char firstLetter = popType && popType[0] ? popType[0] : '?';
        std::string letterStr(1, firstLetter);

        const float textX = boxX + boxSize * k_PopBoxTextXOffsetRatio;
        const float textY = boxY + boxSize * k_PopBoxTextYOffsetRatio;
        rGraphics.DrawText(letterStr, textX, textY, popFontSize, Color::White());
    }
}

void PopulationDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    for (auto it = m_popBoxes.rbegin(); it != m_popBoxes.rend(); ++it)
    {
        if (ContainsMouseCoord(it->bounds, rEvent))
        {
            if (m_onPopClick)
            {
                m_onPopClick(*it->pPop);
            }
            break;
        }
    }
}

} // namespace ac
