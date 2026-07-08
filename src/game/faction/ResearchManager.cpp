#include "game/faction/ResearchManager.h"

#include "game/IEffectsProvider.h"
#include "game/research/TechCostConfig.h"
#include "lib/effects/ActiveEffect.h"

#include <algorithm>

namespace ac
{

ResearchManager::ResearchManager(const TechRegistry* pTechRegistry,
                                 TechCostCalculator* pTechCostCalculator)
    : m_pTechRegistry(pTechRegistry)
    , m_pTechCostCalculator(pTechCostCalculator)
    , m_discoveredTechs()
    , m_pCurrentResearchTarget(nullptr)
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
    m_pCurrentResearchTarget = m_pTechRegistry->Find(techId);
    if (!m_pCurrentResearchTarget)
    {
        throw std::runtime_error("Unknown tech id '" + techId + "'");
    }
    m_bHasResearchTarget = true;
    RecalculatePointsNeeded();
}

TechId ResearchManager::GetResearchTarget() const
{
    return m_pCurrentResearchTarget ? m_pCurrentResearchTarget->id : TechId{};
}

void ResearchManager::ClearResearchTarget()
{
    m_bHasResearchTarget = false;
    m_pCurrentResearchTarget = nullptr;
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

int ResearchManager::BreakthroughRate(int researchPerTurn) const
{
    if (!m_bHasResearchTarget || researchPerTurn <= 0)
    {
        return -1;
    }

    return (m_pointsNeededForCurrentTech + researchPerTurn - 1) / researchPerTurn;
}

int ResearchManager::GetTurnsUntilBreakthrough(int researchPerTurn) const
{
    if (!m_bHasResearchTarget || researchPerTurn <= 0)
    {
        return -1;
    }

    const int remainingPoints = std::max(0, m_pointsNeededForCurrentTech - m_accumulatedPoints);
    if (remainingPoints == 0)
    {
        return 0;
    }

    return (remainingPoints + researchPerTurn - 1) / researchPerTurn;
}

void ResearchManager::RecalculatePointsNeeded()
{
    if (!m_bHasResearchTarget || !m_pTechCostCalculator || !m_pTechRegistry || !m_pCurrentResearchTarget)
    {
        throw std::runtime_error("ResearchManager::RecalculatePointsNeeded: Invalid state");
    }

    TechCostInputs_t inputs;
    inputs.techs      = static_cast<int>(m_discoveredTechs.size()); // TODO: subtract starting techs; add unknownVarA - unknownVarB
    inputs.mostTechs  = inputs.techs;                               // TODO: max across all factions + unknownVarA
    if (m_pEffectsProvider)
    {
        const FactionEffects_t factionEffects = m_pEffectsProvider->GetActiveEffects();
        inputs.factionTechCostModifier = static_cast<int>(
            ResolveStatModifiers(FilterByStatId(factionEffects.effects, StatId::TechCost), 0.0).total);
    }
    // All other fields are placeholder defaults (diff=1, turns=0, bIsAI=false, etc.)

    m_pointsNeededForCurrentTech = m_pTechCostCalculator->CalculateCost(*m_pCurrentResearchTarget, inputs);
}

void ResearchManager::SetEffectsProvider(const IEffectsProvider* pEffectsProvider)
{
    m_pEffectsProvider = pEffectsProvider;
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

    AddDiscoveredTech(m_pCurrentResearchTarget->id);
    m_accumulatedPoints = m_accumulatedPoints - m_pointsNeededForCurrentTech;
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

    std::vector<TechId> available;
    const auto& allConfigs = m_pTechRegistry->GetAll();

    for (const auto& config : allConfigs)
    {
        TechId techId = config.id;

        // Skip if already discovered
        bool bAlreadyDiscovered = false;
        for (TechId discovered : m_discoveredTechs)
        {
            if (discovered == techId)
            {
                bAlreadyDiscovered = true;
                break;
            }
        }

        if (bAlreadyDiscovered)
        {
            continue;
        }

        // Check if all prerequisites are met
        bool bPrerequisitesMet = true;
        for (TechId prereq : config.prerequisites)
        {
            bool bHasPrereq = false;
            for (TechId discovered : m_discoveredTechs)
            {
                if (discovered == prereq)
                {
                    bHasPrereq = true;
                    break;
                }
            }
            if (!bHasPrereq)
            {
                bPrerequisitesMet = false;
                break;
            }
        }

        if (bPrerequisitesMet)
        {
            available.push_back(techId);
        }
    }

    return available;
}

void ResearchManager::ResetAccumulatedPoints_()
{
    m_accumulatedPoints = 0;
}

} // namespace ac
