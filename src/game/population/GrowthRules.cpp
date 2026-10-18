#include "game/population/GrowthRules.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/population/GrowthConfigParser.h"

#include <algorithm>
#include <limits>

namespace ac
{

int ComputeNutrientsRequired(const GrowthConfig_t& rConfig, int baseSize,
                             const BaseEffects_t& rBaseEffects)
{
    // GrowthRate is RawScaled (see KindFor): modifiers scale the 100% baseline seed.
    // TODO: consume RuleFlagId_t::NearZeroGrowth / PopulationBoom (emitted by the growth
    // rating's extreme levels in config) once their gameplay rules are defined.
    const double growthRate = ResolveBaseStat(rBaseEffects, StatId_t::GrowthRate, 100.0);

    // GrowthRate ≤ 0 is not a defined nutrient-threshold rule (SMAC uses the
    // NearZeroGrowth flag at the extreme instead of a zero/negative multiplier).
    // Do not silently treat it as normal growth — block threshold-based growth.
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

GrowthOutcome_t ResolveGrowth(const GrowthInputs_t& rInputs, const GrowthConfig_t& rConfig)
{
    GrowthOutcome_t outcome;
    int stockpile = rInputs.stockpile;
    int net = rInputs.netNutrients;
    const bool bBoxFull = stockpile >= rInputs.boxNow && net >= 0;

    if (bBoxFull && rInputs.bRoomToGrow)
    {
        outcome.bGrows = true;
        outcome.stockpileOnGrowth = 0;
        stockpile = outcome.stockpileOnGrowth;
        // The intake above was for the old size, and the new citizen eats this turn too.
        net -= rConfig.nutrientIntakePerCitizen;
    }
    else if (bBoxFull)
    {
        stockpile = rInputs.boxNow / 2;
    }
    else if (stockpile + net < 0)
    {
        outcome.bStarves = true;
        outcome.stockpileAfter = 0;
        return outcome;
    }

    const int box = outcome.bGrows ? rInputs.boxNext : rInputs.boxNow;
    outcome.stockpileAfter = std::clamp(stockpile + net, 0, box);
    return outcome;
}

} // namespace ac
