#pragma once

#include "game/research/TechRegistry.h"
#include "game/research/TechCostCalculator.h"
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

    int GetPointsNeededForCurrentTech() const;
    void RecalculatePointsNeeded();

    // Full turns to complete the current target at researchPerTurn, ignoring accumulated
    // progress. Returns -1 when there is no target or researchPerTurn <= 0.
    int BreakthroughRate(int researchPerTurn) const;

    // Turns remaining until the current target is discovered at researchPerTurn.
    // Returns -1 when there is no target or researchPerTurn <= 0.
    int GetTurnsUntilBreakthrough(int researchPerTurn) const;

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
    int m_pointsNeededForCurrentTech;

    bool m_bHasResearchTarget;

    void ResetAccumulatedPoints_();
};

} // namespace ac
