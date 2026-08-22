#include "game/faction/ResearchManager.h"

#include "game/IEffectsProvider.h"
#include "game/research/TechCostConfig.h"
#include "game/effects/ActiveEffect.h"

#include <algorithm>
#include <optional>

namespace ac
{

ResearchManager::ResearchManager(const TechRegistry& rTechRegistry,
                                 const TechCostCalculator& rTechCostCalculator,
                                 const IEffectsProvider* pEffectsProvider)
    : m_rTechRegistry(rTechRegistry)
    , m_rTechCostCalculator(rTechCostCalculator)
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
    const TechConfig_t* pTarget = m_rTechRegistry.Find(techId);
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

const std::string& ResearchManager::GetResearchTargetName() const
{
    static const std::string k_None;
    return m_pCurrentResearchTarget ? m_pCurrentResearchTarget->name : k_None;
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
    if (!m_pCurrentResearchTarget)
    {
        throw std::runtime_error(
            "ResearchManager::RecalculatePointsNeeded: no current research target");
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
        inputs.factionTechCostModifier = FinalizeResolvedStat(
            ResolveStatModifiers(FilterByStatId(rFactionEffects.effects, StatId_t::TechCost),
                                 SeedFor(StatId_t::TechCost))
                .total);
        inputs.diff = FinalizeResolvedStat(
            ResolveStatModifiers(FilterByStatId(rFactionEffects.effects, StatId_t::TechCostDiff),
                                 SeedFor(StatId_t::TechCostDiff))
                .total);
        m_costEffectsVersion = m_pEffectsProvider->GetEffectsVersion();
    }
    else
    {
        m_costEffectsVersion = 0;
    }
    m_costResearchRevision = GetRevision();
    // Remaining fields are placeholder defaults (turns=0, bIsAI=false, etc.)

    m_pointsNeededForCurrentTech = m_rTechCostCalculator.CalculateCost(*m_pCurrentResearchTarget, inputs);
}

void ResearchManager::RevalidatePointsNeeded_() const
{
    if (!m_pCurrentResearchTarget)
    {
        return;
    }
    const bool bResearchChanged = GetRevision() != m_costResearchRevision;
    const bool bEffectsChanged = m_pEffectsProvider
        && m_pEffectsProvider->GetEffectsVersion() != m_costEffectsVersion;
    if (bResearchChanged || bEffectsChanged)
    {
        ComputePointsNeeded_();
    }
}

bool ResearchManager::CanDiscoverTech() const
{
    // TODO(difficulty): when rules.research_disabled_turns > 0, block discovery/labs for the
    // first N turns of the session (Citizen/Specialist).
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
    m_revision.Bump();
    OnTechDiscovered.Emit(m_discoveredTechs.back());
}

std::vector<const TechConfig_t*> ResearchManager::GetAvailableTechs() const
{
    std::vector<const TechConfig_t*> available;
    const auto& allConfigs = m_rTechRegistry.GetAll();

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
