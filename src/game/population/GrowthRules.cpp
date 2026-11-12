#include "game/population/GrowthRules.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/population/GrowthConfigParser.h"

#include <algorithm>
#include <limits>
#include <optional>

namespace ac
{

namespace
{

bool IsNutrientBoxFull_(int stockpile, int net, int boxNow)
{
    return stockpile >= boxNow && net >= 0;
}

int DepositNutrients_(int stockpile, int net, int box)
{
    const int deposited = stockpile + net;
    if (deposited > box)
    {
        return box;
    }
    return deposited;
}

std::optional<GrowthOutcome_t> ResolveStarvation_(int stockpile, int net)
{
    if (stockpile >= 0)
    {
        return std::nullopt;
    }
    GrowthOutcome_t outcome;
    outcome.bStarves = true;
    outcome.stockpileAfter = net;
    return outcome;
}

std::optional<GrowthOutcome_t> ResolvePopulationBoom_(const GrowthInputs_t& rInputs,
                                                      const GrowthConfig_t& rConfig)
{
    if (!rInputs.bPopulationBoom || !rInputs.bRoomToGrow
        || !MeetsPopulationBoomSurplus(rInputs.netNutrients, rConfig))
    {
        return std::nullopt;
    }
    GrowthOutcome_t outcome;
    outcome.bGrows = true;
    outcome.stockpileOnGrowth = std::min(rInputs.stockpile, rInputs.boxNow);
    outcome.stockpileAfter = outcome.stockpileOnGrowth;
    return outcome;
}

std::optional<GrowthOutcome_t> ResolveNearZeroGrowth_(const GrowthInputs_t& rInputs)
{
    if (!rInputs.bNearZeroGrowth
        || !IsNutrientBoxFull_(rInputs.stockpile, rInputs.netNutrients, rInputs.boxNow))
    {
        return std::nullopt;
    }
    GrowthOutcome_t outcome;
    outcome.stockpileAfter = rInputs.boxNow;
    return outcome;
}

std::optional<GrowthOutcome_t> ResolveThresholdGrowth_(const GrowthInputs_t& rInputs,
                                                       const GrowthConfig_t& rConfig)
{
    if (!rInputs.bRoomToGrow
        || !IsNutrientBoxFull_(rInputs.stockpile, rInputs.netNutrients, rInputs.boxNow))
    {
        return std::nullopt;
    }
    GrowthOutcome_t outcome;
    outcome.bGrows = true;
    outcome.stockpileOnGrowth = 0;
    // The intake above was for the old size, and the new citizen eats this turn too.
    const int adjustedNet = rInputs.netNutrients - rConfig.nutrientIntakePerCitizen;
    outcome.stockpileAfter = DepositNutrients_(0, adjustedNet, rInputs.boxNext);
    return outcome;
}

std::optional<GrowthOutcome_t> ResolveStalled_(const GrowthInputs_t& rInputs)
{
    if (rInputs.bRoomToGrow
        || !IsNutrientBoxFull_(rInputs.stockpile, rInputs.netNutrients, rInputs.boxNow))
    {
        return std::nullopt;
    }
    GrowthOutcome_t outcome;
    outcome.stockpileAfter =
        DepositNutrients_(rInputs.boxNow / 2, rInputs.netNutrients, rInputs.boxNow);
    return outcome;
}

GrowthOutcome_t ResolveDeposit_(const GrowthInputs_t& rInputs)
{
    GrowthOutcome_t outcome;
    outcome.stockpileAfter =
        DepositNutrients_(rInputs.stockpile, rInputs.netNutrients, rInputs.boxNow);
    return outcome;
}

} // namespace

int ComputeNutrientsRequired(const GrowthConfig_t& rConfig, int baseSize,
                             const BaseEffects_t& rBaseEffects)
{
    // GrowthRate scales the 100% baseline (RawScaled).
    const double growthRate = ResolveBaseStat(rBaseEffects, StatId_t::GrowthRate, 100.0, nullptr);

    // GrowthRate ≤ 0 blocks threshold growth (NearZeroGrowth is a separate full-tank pin).
    if (growthRate <= 0.0)
    {
        return std::numeric_limits<int>::max();
    }

    // Widened, because baseSize * nutrientsPerPop can exceed int before the rate divides it
    // back into range.
    const double multiplier = growthRate / 100.0;
    // Tank capacity uses (size + 1): rows needed for the size after the next growth step.
    const double required = static_cast<double>(baseSize + 1)
                            * static_cast<double>(rConfig.nutrientsPerPop) / multiplier;
    if (required >= static_cast<double>(std::numeric_limits<int>::max()))
    {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(required);
}

bool MeetsPopulationBoomSurplus(int netNutrients, const GrowthConfig_t& rConfig)
{
    const int intake = rConfig.nutrientIntakePerCitizen;
    return intake > 0 && netNutrients >= intake;
}

int PrimeNutrientTanks(int stockpile, int netNutrients)
{
    if (netNutrients >= 0 && stockpile < 0)
    {
        return 0;
    }
    if (netNutrients < 0 && stockpile == 0)
    {
        return -1;
    }
    return stockpile;
}

GrowthOutcome_t ResolveGrowth(const GrowthInputs_t& rInputs, const GrowthConfig_t& rConfig)
{
    if (const auto outcome = ResolveStarvation_(rInputs.stockpile, rInputs.netNutrients))
    {
        return *outcome;
    }
    if (const auto outcome = ResolvePopulationBoom_(rInputs, rConfig))
    {
        return *outcome;
    }
    if (const auto outcome = ResolveNearZeroGrowth_(rInputs))
    {
        return *outcome;
    }
    if (const auto outcome = ResolveThresholdGrowth_(rInputs, rConfig))
    {
        return *outcome;
    }
    if (const auto outcome = ResolveStalled_(rInputs))
    {
        return *outcome;
    }
    return ResolveDeposit_(rInputs);
}

} // namespace ac
