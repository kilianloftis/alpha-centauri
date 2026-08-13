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
        // Scale only the extra surcharge term so MultiplyGeometric 0 (Skunkworks) returns
        // ordinary cost rather than zeroing the whole production bill.
        const double scale = ResolveStatModifiers(
            FilterBaseLevelByStatId(rBaseEffects, StatId_t::PrototypeSurchargeScale),
            SeedFor(StatId_t::PrototypeSurchargeScale)).total;
        multiplier *= (1.0 + static_cast<double>(surchargePercent) / 100.0 * scale);
    }

    return std::max(1, static_cast<int>(std::lround(baseCost * multiplier)));
}

} // namespace ac
