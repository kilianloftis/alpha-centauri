#include "game/faction/ResearchManager.h"

#include "game/IEffectsProvider.h"
#include "game/research/TechCostConfig.h"
#include "game/effects/ActiveEffect.h"

#include <algorithm>
#include <optional>

namespace ac
{

ResearchManager::ResearchManager(const TechRegistry* pTechRegistry,
                                 const TechCostCalculator* pTechCostCalculator,
                                 const IEffectsProvider* pEffectsProvider)
    : m_pTechRegistry(pTechRegistry)
    , m_pTechCostCalculator(pTechCostCalculator)
    , m_pEffectsProvider(pEffectsProvider)
    , m_discoveredTechs()
    , m_pCurrentResearchTarget(nullptr)
    , m_accumulatedPoints(0)
    , m_pointsNeededForCurrentTech(0)
{
}

ResearchManager::~ResearchManager()
{
}

void ResearchManager::SetResearchTarget(TechId techId)
{
    const TechConfig_t* pTarget = m_pTechRegistry->Find(techId);
    if (!pTarget)
    {
        throw std::runtime_error("Unknown tech id '" + techId + "'");
    }
    m_pCurrentResearchTarget = pTarget;
    RecalculatePointsNeeded();
}

TechId ResearchManager::GetResearchTarget() const
{
    return m_pCurrentResearchTarget ? m_pCurrentResearchTarget->id : TechId{};
}

void ResearchManager::ClearResearchTarget()
{
    m_pCurrentResearchTarget = nullptr;
}

bool ResearchManager::HasResearchTarget() const
{
    return m_pCurrentResearchTarget != nullptr;
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
    RevalidatePointsNeeded_();
    return m_pointsNeededForCurrentTech;
}

std::optional<int> ResearchManager::BreakthroughRate(int researchPerTurn) const
{
    if (!m_pCurrentResearchTarget || researchPerTurn <= 0)
    {
        return std::nullopt;
    }

    RevalidatePointsNeeded_();
    return (m_pointsNeededForCurrentTech + researchPerTurn - 1) / researchPerTurn;
}

std::optional<int> ResearchManager::GetTurnsUntilBreakthrough(int researchPerTurn) const
{
    if (!m_pCurrentResearchTarget || researchPerTurn <= 0)
    {
        return std::nullopt;
    }

    RevalidatePointsNeeded_();
    const int remainingPoints = std::max(0, m_pointsNeededForCurrentTech - m_accumulatedPoints);
    if (remainingPoints == 0)
    {
        return 0;
    }

    return (remainingPoints + researchPerTurn - 1) / researchPerTurn;
}

void ResearchManager::RecalculatePointsNeeded()
{
    if (!m_pCurrentResearchTarget || !m_pTechCostCalculator || !m_pTechRegistry)
    {
        throw std::runtime_error("ResearchManager::RecalculatePointsNeeded: Invalid state");
    }
    ComputePointsNeeded_();
}

void ResearchManager::ComputePointsNeeded_() const
{
    TechCostInputs_t inputs;
    inputs.techs      = static_cast<int>(m_discoveredTechs.size()); // TODO: subtract starting techs; add unknownVarA - unknownVarB
    inputs.mostTechs  = inputs.techs;                               // TODO: max across all factions + unknownVarA
    if (m_pEffectsProvider)
    {
        const FactionEffects_t& rFactionEffects = m_pEffectsProvider->GetActiveEffects();
        inputs.factionTechCostModifier = static_cast<int>(
            ResolveStatModifiers(FilterByStatId(rFactionEffects.effects, StatId::TechCost), 0.0).total);
        m_costEffectsVersion = m_pEffectsProvider->GetEffectsVersion();
    }
    // All other fields are placeholder defaults (diff=1, turns=0, bIsAI=false, etc.)

    m_pointsNeededForCurrentTech = m_pTechCostCalculator->CalculateCost(*m_pCurrentResearchTarget, inputs);
}

void ResearchManager::RevalidatePointsNeeded_() const
{
    if (!m_pCurrentResearchTarget || !m_pEffectsProvider)
    {
        return;
    }
    if (m_pEffectsProvider->GetEffectsVersion() != m_costEffectsVersion)
    {
        ComputePointsNeeded_();
    }
}

bool ResearchManager::CanDiscoverTech() const
{
    if (!m_pCurrentResearchTarget)
    {
        return false;
    }
    RevalidatePointsNeeded_();
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

std::vector<const TechConfig_t*> ResearchManager::GetAvailableTechs() const
{
    if (!m_pTechRegistry)
    {
        return {};
    }

    std::vector<const TechConfig_t*> available;
    const auto& allConfigs = m_pTechRegistry->GetAll();

    for (const TechConfig_t& rConfig : allConfigs)
    {
        if (HasDiscoveredTech(rConfig.id))
        {
            continue;
        }

        bool bPrerequisitesMet = true;
        for (const TechId& rPrereq : rConfig.prerequisites)
        {
            if (!HasDiscoveredTech(rPrereq))
            {
                bPrerequisitesMet = false;
                break;
            }
        }

        if (bPrerequisitesMet)
        {
            available.push_back(&rConfig);
        }
    }

    return available;
}

void ResearchManager::ResetAccumulatedPoints_()
{
    m_accumulatedPoints = 0;
}

} // namespace ac
