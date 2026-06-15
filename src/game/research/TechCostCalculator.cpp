#include "game/research/TechCostCalculator.h"
#include "game/research/TechRegistry.h"

namespace ac
{

TechCostCalculator::TechCostCalculator(const TechRegistry* pTechRegistry)
    : m_pTechRegistry(pTechRegistry)
    , m_costMultiplier(1.0f)
    , m_minCost(10)
{
}

TechCostCalculator::~TechCostCalculator()
{
}

int TechCostCalculator::CalculateCost(TechId techId, const std::vector<TechId>& discoveredTechs) const
{
    if (!m_pTechRegistry)
    {
        return m_minCost;
    }

    const Tech* pTech = m_pTechRegistry->GetTech(techId);
    if (!pTech)
    {
        return m_minCost;
    }

    return CalculateCost(pTech, discoveredTechs);
}

int TechCostCalculator::CalculateCost(const Tech* pTech, const std::vector<TechId>& discoveredTechs) const
{
    if (!pTech)
    {
        return m_minCost;
    }

    int baseCost = pTech->GetBaseCost();
    int missingPrereqs = CountMissingPrerequisites_(pTech, discoveredTechs);

    float cost = static_cast<float>(baseCost);
    cost *= (1.0f + 0.5f * missingPrereqs);
    cost *= m_costMultiplier;

    int finalCost = static_cast<int>(cost);
    if (finalCost < m_minCost)
    {
        finalCost = m_minCost;
    }

    return finalCost;
}

void TechCostCalculator::SetCostMultiplier(float multiplier)
{
    m_costMultiplier = multiplier;
}

float TechCostCalculator::GetCostMultiplier() const
{
    return m_costMultiplier;
}

void TechCostCalculator::SetMinCost(int minCost)
{
    m_minCost = minCost;
}

int TechCostCalculator::GetMinCost() const
{
    return m_minCost;
}

int TechCostCalculator::CountMissingPrerequisites_(const Tech* pTech, const std::vector<TechId>& discoveredTechs) const
{
    int missing = 0;
    for (TechId prereq : pTech->GetPrerequisites())
    {
        if (!IsTechDiscovered_(prereq, discoveredTechs))
        {
            missing++;
        }
    }
    return missing;
}

bool TechCostCalculator::IsTechDiscovered_(TechId techId, const std::vector<TechId>& discoveredTechs) const
{
    for (TechId discovered : discoveredTechs)
    {
        if (discovered == techId)
        {
            return true;
        }
    }
    return false;
}

} // namespace ac
