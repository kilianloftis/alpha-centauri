#pragma once

#include "game/research/Tech.h"
#include <vector>

namespace ac
{

class TechRegistry;

class TechCostCalculator
{
public:
    explicit TechCostCalculator(const TechRegistry* pTechRegistry);
    ~TechCostCalculator();

    int CalculateCost(TechId techId, const std::vector<TechId>& discoveredTechs) const;
    int CalculateCost(const Tech* pTech, const std::vector<TechId>& discoveredTechs) const;

    void SetCostMultiplier(float multiplier);
    float GetCostMultiplier() const;

    void SetMinCost(int minCost);
    int GetMinCost() const;

private:
    const TechRegistry* m_pTechRegistry;
    float m_costMultiplier;
    int m_minCost;

    int CountMissingPrerequisites_(const Tech* pTech, const std::vector<TechId>& discoveredTechs) const;
    bool IsTechDiscovered_(TechId techId, const std::vector<TechId>& discoveredTechs) const;
};

} // namespace ac
