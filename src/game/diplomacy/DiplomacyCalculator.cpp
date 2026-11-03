#include "game/diplomacy/DiplomacyCalculator.h"

#include "game/diplomacy/DiplomacyConfig.h"
#include "game/faction/FactionConfig.h"
#include "game/research/TechConfigParser.h"

#include <algorithm>
#include <stdexcept>

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
    return rConfig.ungarrisonedFactor.ScaledTruncated(value);
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
        scaled = rConfig.outnumberedFactor.ScaledTruncated(scaled);
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

// Each threshold truncates as configured; the giver's scale then applies unrounded.
bool MeetsThresholds_(const GiftSizeStepConfig_t& rStep, const GiftSizeInputs_t& rInputs)
{
    const int thresholds[] = {
        rStep.recipientCreditsFactor.ScaledTruncated(rInputs.recipientCredits),
        rStep.giverCreditsFactor.ScaledTruncated(rInputs.giverCredits),
        rStep.turnFactor.ScaledTruncated(rInputs.turn),
        rStep.minimum,
    };
    return std::ranges::all_of(thresholds, [&rInputs](int threshold)
                               { return rInputs.value >= threshold * rInputs.thresholdScale; });
}

GiftSize_t SizeOnLadder_(const GiftSizesConfig_t& rSizes, const GiftSizeInputs_t& rInputs)
{
    for (const GiftSizeStepConfig_t& rStep : rSizes.steps)
    {
        if (MeetsThresholds_(rStep, rInputs))
        {
            return rStep.size;
        }
    }
    return rSizes.fallback;
}

// The first block of units divides by its divisor, each later block by the next one up.
int TieredUnitWeight_(const CreditFrictionConfig_t& rConfig, int units)
{
    const int firstBlock = std::min(units, rConfig.firstBlockUnits);
    int weight = firstBlock / rConfig.firstBlockDivisor;
    int divisor = rConfig.firstBlockDivisor;
    for (int remaining = units - firstBlock; remaining > 0; remaining -= rConfig.blockUnits)
    {
        ++divisor;
        weight += std::min(remaining, rConfig.blockUnits) / divisor;
    }
    return weight;
}

int Weighted_(const TechValueConfig_t& rConfig, int weight, bool bPriority)
{
    const int priority = bPriority ? 1 : 0;
    return weight * (rConfig.priorityFactor * priority + 1);
}

bool MatchesPriority_(const TechValueConfig_t& rConfig, const TechAiWeights_t& rWeights,
                      const AITendenciesConfig& rPriorities, int vendettaCount, int baseCount)
{
    const bool bPower = rWeights.power > 0 && (rPriorities.power || vendettaCount > 0);
    const bool bTech = rWeights.tech > 0 && rPriorities.tech;
    const bool bWealth = rWeights.wealth > 0 && rPriorities.wealth;
    const bool bGrowth =
        rWeights.growth > 0 && (rPriorities.growth || baseCount < rConfig.growthBaseCount);
    return bPower || bTech || bWealth || bGrowth;
}

// The first step counts only a generous gift; the second also counts a modest one, and a
// notable one from 0.
bool TakesSecondGiftStep_(GiftSize_t size, int current)
{
    switch (size)
    {
    case GiftSize_t::Small:
        return false;
    case GiftSize_t::Notable:
        return current == 0;
    case GiftSize_t::Modest:
    case GiftSize_t::Generous:
        return true;
    }
    throw std::logic_error("TakesSecondGiftStep_: unhandled GiftSize_t");
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

GiftSize_t DiplomacyCalculator::CreditGiftSize(const GiftSizeInputs_t& rInputs) const
{
    return SizeOnLadder_(m_rConfig.gifts.credits.sizes, rInputs);
}

int DiplomacyCalculator::CreditGiftFriction(int value) const
{
    const CreditFrictionConfig_t& rConfig = m_rConfig.gifts.credits.friction;
    const int weight = TieredUnitWeight_(rConfig, value / rConfig.creditsPerUnit);
    const int points = (weight + rConfig.unitsPerPoint - 1) / rConfig.unitsPerPoint;
    return -rConfig.factor.ScaledTruncated(points);
}

GiftSize_t DiplomacyCalculator::TechGiftSize(const GiftSizeInputs_t& rInputs) const
{
    return SizeOnLadder_(m_rConfig.gifts.technology.sizes, rInputs);
}

int DiplomacyCalculator::TechGiftFriction(int value) const
{
    return -m_rConfig.gifts.technology.frictionFactor.ScaledTruncated(value);
}

GiftSize_t DiplomacyCalculator::BaseGiftSize(const GiftSizeInputs_t& rInputs) const
{
    return SizeOnLadder_(m_rConfig.gifts.base.sizes, rInputs);
}

int DiplomacyCalculator::BaseGiftFriction(int value) const
{
    return -m_rConfig.gifts.base.frictionFactor.ScaledTruncated(value);
}

int DiplomacyCalculator::TechWeightValue(const TechAiWeights_t& rWeights,
                                         const AITendenciesConfig& rPriorities,
                                         int vendettaCount, int baseCount) const
{
    const TechValueConfig_t& rConfig = m_rConfig.gifts.technology.value;
    const int sum = Weighted_(rConfig, rWeights.power, rPriorities.power)
        + Weighted_(rConfig, rWeights.tech, rPriorities.tech)
        + Weighted_(rConfig, rWeights.wealth, rPriorities.wealth)
        + Weighted_(rConfig, rWeights.growth, rPriorities.growth);
    if (MatchesPriority_(rConfig, rWeights, rPriorities, vendettaCount, baseCount))
    {
        return sum;
    }
    return rConfig.unmatchedFactor.ScaledRoundedUp(sum);
}

int DiplomacyCalculator::UnownedTechValue(int value, int owners, int techLevel,
                                          int highestLevel) const
{
    const TechValueConfig_t& rConfig = m_rConfig.gifts.technology.value;
    int adjusted = value;
    if (owners > 1)
    {
        adjusted -= rConfig.extraOwnerPenalty * (owners - 1);
    }
    if (techLevel < highestLevel)
    {
        adjusted = adjusted * (techLevel + 1) / (highestLevel + 1);
    }
    return std::max(adjusted, rConfig.minimum);
}

int DiplomacyCalculator::NextGifts(int current, int giverBlemishes, GiftSize_t size) const
{
    int next = current;
    if (size == GiftSize_t::Generous && next < giverBlemishes)
    {
        ++next;
    }
    if (TakesSecondGiftStep_(size, current) && next < giverBlemishes)
    {
        ++next;
    }
    return next;
}

int DiplomacyCalculator::AlliedTerritoryGifts(int current) const
{
    return std::max(current, m_rConfig.alliedTerritoryBattleGiftsFloor);
}

} // namespace ac
