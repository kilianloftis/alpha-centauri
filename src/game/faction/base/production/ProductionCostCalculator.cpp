#include "game/faction/base/production/ProductionCostCalculator.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include <algorithm>
#include <cmath>

namespace ac
{

int ProductionCostCalculator::ComputeCost(int baseCost, const BaseEffects_t& rBaseEffects)
{
    // CostMultiplier is PureMultiplier (seed 1.0). Industry rating levels emit AddPercent
    // contributions here via ExpandSocialRatingEffects — same seam as GrowthRate.
    const double multiplier = ResolveStatModifiers(
        FilterFlatByStatId(rBaseEffects, StatId_t::CostMultiplier),
        SeedFor(StatId_t::CostMultiplier)).total;

    return std::max(1, static_cast<int>(std::lround(baseCost * k_MineralsPerRow * multiplier)));
}

} // namespace ac
