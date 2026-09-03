#include "ui/base/PopulationDisplay.h"
#include "graphics/Graphics.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/population/pop-types/Pop.h"
#include "ui/style/UiStyle.h"
#include <algorithm>
#include <sstream>

namespace ac
{

namespace
{

constexpr int k_DroneGroupOrder              = 0;
constexpr int k_WorkerGroupOrder             = 1;
constexpr int k_TalentGroupOrder             = 2;
constexpr int k_SpecialistGroupOrder         = 3;
constexpr size_t k_MinPopCountForSpacing     = 2;

int PopGroupOrder(const Pop& rPop)
{
    if (rPop.IsDrone())
        return k_DroneGroupOrder;
    if (rPop.IsPlainWorker())
        return k_WorkerGroupOrder;
    if (rPop.IsTalent())
        return k_TalentGroupOrder;
    return k_SpecialistGroupOrder;
}

int SpecialistTotalOutput(const Pop& rPop)
{
    const SpecialistOutput_t out = rPop.GetSpecialistOutput();
    return out.econ + out.labs + out.psych;
}

} // namespace

PopulationDisplay::PopulationDisplay(PopulationManager& rPopulation, WindowLayout_t layout,
                                     PopClickCallback_t onPopClick)
    : UIElement(layout)
    , m_rPopulation(rPopulation)
    , m_onPopClick(std::move(onPopClick))
{}

void PopulationDisplay::Render(Graphics& rGraphics)
{
    const auto& style = Style().populationDisplay;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);

    std::ostringstream oss;
    oss << "Population: " << m_rPopulation.GetSize();
    if (m_rPopulation.IsRioting())
    {
        oss << "  [Riot T" << m_rPopulation.GetConsecutiveRiotTurns() << "]";
    }
    else if (m_rPopulation.IsPendingRiot())
    {
        oss << "  [Riot pending]";
    }
    if (m_rPopulation.IsInGoldenAge())
    {
        oss << "  [Golden Age]";
    }
    const float leftPadding = m_layout.width * style.leftPaddingRatio;
    rGraphics.DrawText(oss.str(), m_layout.x + leftPadding, m_layout.y, headerFontSize, style.headerTextColor);

    const float boxSize = m_layout.height * style.popBoxSizeRatio;
    const float popFontSize = static_cast<unsigned int>(boxSize * style.popBoxFontSizeRatio);
    const float startY = m_layout.y + headerFontSize + m_layout.height * style.popRowYOffsetRatio;

    m_popBoxes.clear();

    std::vector<Pop*> sortedPops;
    sortedPops.reserve(m_rPopulation.GetSize());
    for (Pop& rPop : m_rPopulation.Pops())
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
    float boxSpacing = m_layout.height * style.popBoxSpacingRatio;
    if (sortedPops.size() >= k_MinPopCountForSpacing && totalBoxWidth > m_layout.width)
    {
        boxSpacing = (m_layout.width - totalBoxWidth) / (sortedPops.size() - 1);
    }

    for (size_t i = 0; i < sortedPops.size(); ++i)
    {
        const float boxX = m_layout.x + leftPadding + i * (boxSize + boxSpacing);
        const float boxY = startY;

        rGraphics.DrawFilledRect(boxX, boxY, boxSize, boxSize, style.popBoxFillColor);
        rGraphics.DrawRect(boxX, boxY, boxSize, boxSize, style.popBoxBorderColor, style.popBoxBorderWidth);

        m_popBoxes.push_back(PopBox_t{{boxX, boxY, boxSize, boxSize}, sortedPops[i]});

        const std::string letterStr(1, sortedPops[i]->GetConfig().displayGlyph);

        const float textX = boxX + boxSize * style.popBoxTextXOffsetRatio;
        const float textY = boxY + boxSize * style.popBoxTextYOffsetRatio;
        rGraphics.DrawText(letterStr, textX, textY, popFontSize, style.popLetterColor);
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
