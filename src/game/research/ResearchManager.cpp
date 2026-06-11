#include "game/research/ResearchManager.h"

namespace ac
{

ResearchManager::ResearchManager()
    : m_pTechRegistry(nullptr)
    , m_pTechCostCalculator(std::make_unique<TechCostCalculator>())
    , m_discoveredTechs()
    , m_currentResearchTarget(0)
    , m_accumulatedPoints(0)
    , m_pointsNeededForCurrentTech(0)
    , m_bHasResearchTarget(false)
{
}

ResearchManager::ResearchManager(const TechRegistry* pTechRegistry)
    : m_pTechRegistry(pTechRegistry)
    , m_pTechCostCalculator(std::make_unique<TechCostCalculator>(pTechRegistry))
    , m_discoveredTechs()
    , m_currentResearchTarget(0)
    , m_accumulatedPoints(0)
    , m_pointsNeededForCurrentTech(0)
    , m_bHasResearchTarget(false)
{
}

ResearchManager::~ResearchManager()
{
}

void ResearchManager::SetTechRegistry(const TechRegistry* pTechRegistry)
{
    m_pTechRegistry = pTechRegistry;
    if (m_pTechCostCalculator)
    {
        m_pTechCostCalculator->SetTechRegistry(pTechRegistry);
    }
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
    m_currentResearchTarget = 0;
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
    if (!m_bHasResearchTarget || !m_pTechCostCalculator)
    {
        m_pointsNeededForCurrentTech = 0;
        return;
    }

    m_pointsNeededForCurrentTech = m_pTechCostCalculator->CalculateCost(m_currentResearchTarget, m_discoveredTechs);
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
    if (!HasDiscoveredTech(techId))
    {
        m_discoveredTechs.push_back(techId);
    }
}

std::vector<TechId> ResearchManager::GetAvailableResearchTargets() const
{
    if (!m_pTechRegistry)
    {
        return std::vector<TechId>();
    }

    return m_pTechRegistry->GetAvailableTechs(m_discoveredTechs);
}

void ResearchManager::SetTechCostCalculator(std::unique_ptr<TechCostCalculator> pCalculator)
{
    m_pTechCostCalculator = std::move(pCalculator);
}

TechCostCalculator* ResearchManager::GetTechCostCalculator()
{
    return m_pTechCostCalculator.get();
}

void ResearchManager::ResetAccumulatedPoints_()
{
    m_accumulatedPoints = 0;
}

} // namespace ac
