#include "game/research/ResearchManager.h"

#include "game/research/ResearchRules.h"

#include "game/effects/IEffectsProvider.h"
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
    if (!m_pCurrentResearchTarget)
    {
        return 0;
    }
    return ScaledTechCost(FormulaCost_(), m_pEffectsProvider, *m_pCurrentResearchTarget);
}

int ResearchManager::FormulaCost_() const
{
    if (m_breakthroughFormulaCost)
    {
        return *m_breakthroughFormulaCost;
    }
    return ResearchFormulaCost(*this, m_pEffectsProvider, m_rTechCostCalculator);
}

std::optional<int> ResearchManager::GetTurnsUntilBreakthrough(int researchPerTurn) const
{
    if (!m_pCurrentResearchTarget || researchPerTurn <= 0)
    {
        return std::nullopt;
    }

    const int remainingPoints = std::max(0, GetPointsNeededForCurrentTech() - m_accumulatedPoints);
    if (remainingPoints == 0)
    {
        return 0;
    }

    return (remainingPoints + researchPerTurn - 1) / researchPerTurn;
}

bool ResearchManager::CanDiscoverTech() const
{
    // TODO(difficulty): when rules.research_disabled_turns > 0, block discovery/labs for the
    // first N turns of the session (Citizen/Specialist).
    if (!m_pCurrentResearchTarget)
    {
        return false;
    }
    return m_accumulatedPoints >= GetPointsNeededForCurrentTech();
}

bool ResearchManager::DiscoverTech()
{
    if (!CanDiscoverTech())
    {
        return false;
    }

    // terranx tech_research zeroes the surplus and prices the next tech now.
    AddDiscoveredTech(m_pCurrentResearchTarget->id);
    m_accumulatedPoints = 0;
    ClearResearchTarget();
    m_breakthroughFormulaCost = ResearchFormulaCost(*this, m_pEffectsProvider, m_rTechCostCalculator);

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

} // namespace ac
