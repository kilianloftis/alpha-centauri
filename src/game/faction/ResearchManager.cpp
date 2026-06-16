#include "game/faction/ResearchManager.h"
#include "game/research/TechCostConfig.h"

namespace ac
{

ResearchManager::ResearchManager(const TechRegistry* pTechRegistry,
                                 TechCostCalculator* pTechCostCalculator)
    : m_pTechRegistry(pTechRegistry)
    , m_pTechCostCalculator(pTechCostCalculator)
    , m_discoveredTechs()
    , m_currentResearchTarget()
    , m_accumulatedPoints(0)
    , m_pointsNeededForCurrentTech(0)
    , m_bHasResearchTarget(false)
{
}

ResearchManager::~ResearchManager()
{
}

void ResearchManager::SetResearchTarget(TechId techId)
{
    m_currentResearchTarget = techId;
    m_bHasResearchTarget = true;
    RecalculatePointsNeeded();
}

TechId ResearchManager::GetResearchTarget() const
{
    return m_currentResearchTarget;
}

void ResearchManager::ClearResearchTarget()
{
    m_bHasResearchTarget = false;
    m_currentResearchTarget.clear();
}

bool ResearchManager::HasResearchTarget() const
{
    return m_bHasResearchTarget;
}

int ResearchManager::GetAccumulatedPoints() const
{
    return m_accumulatedPoints;
}

void ResearchManager::AddResearchPoints(int points)
{
    m_accumulatedPoints += points;
}

void ResearchManager::SetAccumulatedPoints(int points)
{
    m_accumulatedPoints = points;
}

int ResearchManager::GetPointsNeededForCurrentTech() const
{
    return m_pointsNeededForCurrentTech;
}

void ResearchManager::RecalculatePointsNeeded()
{
    if (!m_bHasResearchTarget || !m_pTechCostCalculator || !m_pTechRegistry)
    {
        throw std::runtime_error("ResearchManager::RecalculatePointsNeeded: Invalid state");
    }

    const Tech* pTech = m_pTechRegistry->GetTech(m_currentResearchTarget);
    if (!pTech)
    {
        throw std::runtime_error("ResearchManager::RecalculatePointsNeeded: Tech not found");
    }

    TechCostInputs_t inputs;
    inputs.techs      = static_cast<int>(m_discoveredTechs.size()); // TODO: subtract starting techs; add unknownVarA - unknownVarB
    inputs.mostTechs  = inputs.techs;                               // TODO: max across all factions + unknownVarA
    // All other fields are placeholder defaults (diff=1, turns=0, bIsAI=false, etc.)

    m_pointsNeededForCurrentTech = m_pTechCostCalculator->CalculateCost(*pTech, inputs);
}

bool ResearchManager::CanDiscoverTech() const
{
    if (!m_bHasResearchTarget)
    {
        return false;
    }
    return m_accumulatedPoints >= m_pointsNeededForCurrentTech;
}

bool ResearchManager::DiscoverTech()
{
    if (!CanDiscoverTech())
    {
        return false;
    }

    AddDiscoveredTech(m_currentResearchTarget);
    ResetAccumulatedPoints_();
    ClearResearchTarget();

    return true;
}

const std::vector<TechId>& ResearchManager::GetDiscoveredTechs() const
{
    return m_discoveredTechs;
}

bool ResearchManager::HasDiscoveredTech(TechId techId) const
{
    for (TechId discovered : m_discoveredTechs)
    {
        if (discovered == techId)
        {
            return true;
        }
    }
    return false;
}

void ResearchManager::AddDiscoveredTech(TechId techId)
{
    if (HasDiscoveredTech(techId))
    {
        throw std::invalid_argument("ResearchManager::AddDiscoveredTech: Tech already discovered");
    }
    m_discoveredTechs.push_back(techId);
}

std::vector<TechId> ResearchManager::GetAvailableResearchTargets() const
{
    if (!m_pTechRegistry)
    {
        return std::vector<TechId>();
    }

    return m_pTechRegistry->GetAvailableTechs(m_discoveredTechs);
}

void ResearchManager::ResetAccumulatedPoints_()
{
    m_accumulatedPoints = 0;
}

} // namespace ac
