#pragma once

#include "game/research/Tech.h"
#include "game/research/TechRegistry.h"
#include "game/research/TechCostCalculator.h"
#include <vector>
#include <memory>

namespace ac
{

class ResearchManager
{
public:
    ResearchManager();
    explicit ResearchManager(const TechRegistry* pTechRegistry);
    ~ResearchManager();

    void SetTechRegistry(const TechRegistry* pTechRegistry);

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

    void SetTechCostCalculator(std::unique_ptr<TechCostCalculator> pCalculator);
    TechCostCalculator* GetTechCostCalculator();

private:
    const TechRegistry* m_pTechRegistry;
    std::unique_ptr<TechCostCalculator> m_pTechCostCalculator;

    std::vector<TechId> m_discoveredTechs;
    TechId m_currentResearchTarget;
    int m_accumulatedPoints;
    int m_pointsNeededForCurrentTech;

    bool m_bHasResearchTarget;

    void ResetAccumulatedPoints_();
};

} // namespace ac
