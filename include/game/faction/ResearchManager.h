#pragma once

#include "game/research/TechRegistry.h"
#include "game/research/TechCostCalculator.h"
#include <vector>

namespace ac
{

class ResearchManager
{
public:
    ResearchManager(const TechRegistry* pTechRegistry, TechCostCalculator* pTechCostCalculator);
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

    bool CanDiscoverTech() const;
    bool DiscoverTech();

    const std::vector<TechId>& GetDiscoveredTechs() const;
    bool HasDiscoveredTech(TechId techId) const;
    void AddDiscoveredTech(TechId techId);

    std::vector<TechId> GetAvailableResearchTargets() const;

private:
    const TechRegistry* m_pTechRegistry;
    TechCostCalculator* m_pTechCostCalculator;

    std::vector<TechId> m_discoveredTechs;
    const TechConfig_t* m_pCurrentResearchTarget;
    int m_accumulatedPoints;
    int m_pointsNeededForCurrentTech;

    bool m_bHasResearchTarget;

    void ResetAccumulatedPoints_();
};

} // namespace ac
