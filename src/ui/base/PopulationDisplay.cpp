#include "ui/base/PopulationDisplay.h"
#include "graphics/Graphics.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/population/pop-types/Pop.h"
#include <algorithm>
#include <sstream>

namespace ac
{

namespace
{

constexpr int k_DroneGroupOrder              = 0;
constexpr int k_WorkerGroupOrder             = 1;
constexpr int k_GoldenAgeGroupOrder          = 2;
constexpr int k_SpecialistGroupOrder         = 3;
constexpr int k_NoGoldenAgeContribution      = 0;
constexpr Color_t k_BackgroundColor            {20, 20, 20, 255};
constexpr float k_HeaderFontSizeRatio        = 0.04f;
constexpr float k_PopBoxSizeRatio            = 0.6f;
constexpr float k_PopBoxSpacingRatio         = 0.02f;
constexpr float k_LeftPaddingRatio           = 0.02f;
constexpr float k_PopBoxFontSizeRatio        = 0.6f;
constexpr float k_PopRowYOffsetRatio         = 0.02f;
constexpr float k_PopBoxTextXOffsetRatio     = 0.35f;
constexpr float k_PopBoxTextYOffsetRatio     = 0.2f;
constexpr float k_PopBoxBorderWidth          = 2.0f;
constexpr size_t k_MinPopCountForSpacing     = 2;
constexpr char k_UnknownPopLetter            = '?';

int PopGroupOrder(const Pop& rPop)
{
    if (rPop.IsDrone())
        return k_DroneGroupOrder;
    if (rPop.IsWorker() && rPop.GetGoldenAgeContribution() == k_NoGoldenAgeContribution)
        return k_WorkerGroupOrder;
    if (rPop.GetGoldenAgeContribution() > k_NoGoldenAgeContribution)
        return k_GoldenAgeGroupOrder;
    return k_SpecialistGroupOrder;
}

int SpecialistTotalOutput(const Pop& rPop)
{
    const SpecialistOutput_t out = rPop.GetSpecialistOutput();
    return out.econ + out.labs + out.psych;
}

} // namespace

PopulationDisplay::PopulationDisplay(PopulationManager* pPopulation, WindowLayout_t layout, PopClickCallback_t onPopClick)
    : UIElement(layout)
    , m_pPopulation(pPopulation)
    , m_onPopClick(std::move(onPopClick))
{}

void PopulationDisplay::Render(Graphics& rGraphics)
{
    if (!m_pPopulation)
    {
        throw std::runtime_error("PopulationDisplay: No population container set");
    }

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BackgroundColor);

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * k_HeaderFontSizeRatio);

    std::ostringstream oss;
    oss << "Population: " << m_pPopulation->GetSize();
    const float leftPadding = m_layout.width * k_LeftPaddingRatio;
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y, headerFontSize);

    const float boxSize = m_layout.height * k_PopBoxSizeRatio;
    const float popFontSize = static_cast<unsigned int>(boxSize * k_PopBoxFontSizeRatio);
    const float startY = m_layout.y + headerFontSize + m_layout.height * k_PopRowYOffsetRatio;

    m_popBoxes.clear();

    std::vector<Pop*> sortedPops;
    sortedPops.reserve(m_pPopulation->GetSize());
    for (Pop& rPop : m_pPopulation->Pops())
        sortedPops.push_back(&rPop);

    std::stable_sort(sortedPops.begin(), sortedPops.end(),
        [](const Pop* pA, const Pop* pB)
        {
            const int groupA = PopGroupOrder(*pA);
            const int groupB = PopGroupOrder(*pB);
            if (groupA != groupB)
                return groupA < groupB;
            if (groupA == k_SpecialistGroupOrder)
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
    if (sortedPops.size() >= k_MinPopCountForSpacing && totalBoxWidth > m_layout.width)
    {
        boxSpacing = (m_layout.width - totalBoxWidth) / (sortedPops.size() - 1);
    }

    for (size_t i = 0; i < sortedPops.size(); ++i)
    {
        const float boxX = m_layout.x + leftPadding + i * (boxSize + boxSpacing);
        const float boxY = startY;

        rGraphics.DrawFilledRect(boxX, boxY, boxSize, boxSize, Color_t::Blue());
        rGraphics.DrawRect(boxX, boxY, boxSize, boxSize, Color_t::White(), k_PopBoxBorderWidth);

        m_popBoxes.push_back(PopBox_t{{boxX, boxY, boxSize, boxSize}, sortedPops[i]});

        const char* popType = sortedPops[i]->GetPopType();
        char firstLetter = popType && popType[0] ? popType[0] : k_UnknownPopLetter;
        // Letter collisions until pop types carry an explicit display glyph:
        // Drone/Doctor both D → R (riot); Talent/Technician both T → A (tAlent).
        if (sortedPops[i]->IsDrone())
        {
            firstLetter = 'R';
        }
        else if (sortedPops[i]->IsTalent())
        {
            firstLetter = 'A';
        }
        std::string letterStr(1, firstLetter);

        const float textX = boxX + boxSize * k_PopBoxTextXOffsetRatio;
        const float textY = boxY + boxSize * k_PopBoxTextYOffsetRatio;
        rGraphics.DrawText(letterStr, textX, textY, popFontSize, Color_t::White());
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
