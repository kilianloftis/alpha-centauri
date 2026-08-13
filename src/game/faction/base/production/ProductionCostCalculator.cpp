#include "game/faction/base/production/ProductionCostCalculator.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include <algorithm>
#include <cmath>

namespace ac
{

int ProductionCostCalculator::ComputeCost(int baseCost, const BaseEffects_t& rBaseEffects,
                                          int surchargePercent)
{
    // CostMultiplier is PureMultiplier (seed 1.0). Industry rating levels emit AddPercent
    // contributions here via ResolveSocialRatingLevelEffects — same seam as GrowthRate.
    double multiplier = ResolveStatModifiers(
        FilterBaseLevelByStatId(rBaseEffects, StatId_t::CostMultiplier),
        SeedFor(StatId_t::CostMultiplier)).total;

    if (surchargePercent > 0)
    {
        multiplier *= (1.0 + static_cast<double>(surchargePercent) / 100.0);
    }

    return std::max(1, static_cast<int>(std::lround(baseCost * multiplier)));
}

} // namespace ac
