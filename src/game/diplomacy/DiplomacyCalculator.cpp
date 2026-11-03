#include "game/diplomacy/DiplomacyCalculator.h"

#include "game/diplomacy/DiplomacyConfig.h"

#include <algorithm>

namespace ac
{

namespace
{

// terranx-map.md "Base value" steps 3-4.
int SiteValue_(const BaseValueConfig_t& rConfig, const BaseValueInputs_t& rInputs)
{
    const int perPop =
        (rInputs.credits + rConfig.creditsOffset) / (rInputs.distance + rConfig.distanceOffset);
    const int population = std::max(rInputs.population, rConfig.minPopulation);
    const int value = std::max(perPop * population, rConfig.minimum);
    if (rInputs.bGarrisoned)
    {
        return value;
    }
    return value / rConfig.ungarrisonedDivisor;
}

int ScaleByRegionHoldings_(const BaseValueConfig_t& rConfig, const BaseValueInputs_t& rInputs,
                           int value)
{
    const int evaluator = rInputs.evaluatorRegionBases;
    const int counterparty = rInputs.counterpartyRegionBases;
    int scaled = value;
    if (counterparty == 0)
    {
        scaled *= rConfig.counterpartyAbsentMultiplier;
    }
    if (evaluator == 1 && counterparty <= 1)
    {
        scaled *= rConfig.loneBaseMultiplier;
    }
    if (evaluator > 0 && counterparty >= rConfig.outnumberedRatio * evaluator)
    {
        scaled /= rConfig.outnumberedDivisor;
    }
    return scaled;
}

int WeightedFacilityMinerals_(const std::vector<int>& rFacilityMinerals)
{
    int total = 0;
    int weight = 0;
    for (const int minerals : rFacilityMinerals)
    {
        ++weight;
        total += weight * minerals;
    }
    return total;
}

int BuiltValue_(const BaseValueConfig_t& rConfig, const BaseValueInputs_t& rInputs)
{
    const int weightedFacilities = WeightedFacilityMinerals_(rInputs.facilityMinerals);
    const int facilities = weightedFacilities / rConfig.mineralsPerRow;
    const int projects =
        rConfig.projectRowsMultiplier * rInputs.projectMinerals / rConfig.mineralsPerRow;
    const int units =
        rConfig.unitRowsMultiplier * rInputs.evaluatorUnitMinerals / rConfig.mineralsPerRow;
    return facilities + projects + units;
}

} // namespace

DiplomacyCalculator::DiplomacyCalculator(const DiplomacyConfig_t& rConfig)
    : m_rConfig(rConfig)
{}

int DiplomacyCalculator::NextFriction(int current, int delta) const
{
    return std::clamp(current + delta, 0, m_rConfig.frictionMax);
}

int DiplomacyCalculator::NextBlemishes(int current, int delta, int floor) const
{
    const int raised = std::max(current + delta, floor);
    return std::clamp(raised, 0, m_rConfig.blemishesMax);
}

int DiplomacyCalculator::BaseValue(const BaseValueInputs_t& rInputs) const
{
    const BaseValueConfig_t& rConfig = m_rConfig.baseValue;
    const int siteValue = SiteValue_(rConfig, rInputs);
    const int scaled = ScaleByRegionHoldings_(rConfig, rInputs, siteValue);
    return scaled + BuiltValue_(rConfig, rInputs);
}

} // namespace ac
