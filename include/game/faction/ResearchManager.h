#pragma once

#include "game/research/TechRegistry.h"
#include "game/research/TechCostCalculator.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace ac
{

class IEffectsProvider;

class ResearchManager
{
public:
    // pEffectsProvider supplies the faction effect pool for TechCost modifiers; may be null
    // when no effects should influence research cost.
    ResearchManager(const TechRegistry* pTechRegistry,
                    const TechCostCalculator* pTechCostCalculator,
                    const IEffectsProvider* pEffectsProvider);
    ~ResearchManager();

    void SetResearchTarget(TechId techId);
    TechId GetResearchTarget() const;
    void ClearResearchTarget();
    bool HasResearchTarget() const;

    int GetAccumulatedPoints() const;
    void AddResearchPoints(int points);
    void SetAccumulatedPoints(int points);

    // Cost of the current target. Revalidated against the effects provider's version, so a
    // TechCost-modifying effect appearing mid-research is reflected on the next read.
    int GetPointsNeededForCurrentTech() const;
    void RecalculatePointsNeeded();

    // Full turns to complete the current target at researchPerTurn, ignoring accumulated
    // progress. Empty when there is no target or researchPerTurn <= 0.
    std::optional<int> BreakthroughRate(int researchPerTurn) const;

    // Turns remaining until the current target is discovered at researchPerTurn.
    // Empty when there is no target or researchPerTurn <= 0.
    std::optional<int> GetTurnsUntilBreakthrough(int researchPerTurn) const;

    bool CanDiscoverTech() const;
    bool DiscoverTech();

    const std::vector<TechId>& GetDiscoveredTechs() const;
    bool HasDiscoveredTech(TechId techId) const;
    void AddDiscoveredTech(TechId techId);

    std::vector<const TechConfig_t*> GetAvailableTechs() const;

private:
    const TechRegistry* m_pTechRegistry;
    const TechCostCalculator* m_pTechCostCalculator;
    const IEffectsProvider* m_pEffectsProvider;

    std::vector<TechId> m_discoveredTechs;
    const TechConfig_t* m_pCurrentResearchTarget;
    int m_accumulatedPoints;
    mutable int m_pointsNeededForCurrentTech;
    // Provider effects version the cost was computed against (0 = no provider involved).
    mutable uint64_t m_costEffectsVersion = 0;

    void ResetAccumulatedPoints_();

    // Compute the cost of the current target and record the provider version it used.
    void ComputePointsNeeded_() const;

    // Recompute m_pointsNeededForCurrentTech if the provider's effect pool changed since
    // the last computation. No-op without a target or provider.
    void RevalidatePointsNeeded_() const;
};

} // namespace ac
