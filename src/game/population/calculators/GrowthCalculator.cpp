#include "game/population/calculators/GrowthCalculator.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include <limits>

namespace ac
{

int GrowthCalculator::ComputeNutrientsRequired(const GrowthConfig_t& rConfig, int baseSize, const BaseEffects_t& rBaseEffects)
{
    // GrowthRate is RawScaled (see KindFor): modifiers scale the 100% baseline seed.
    // TODO: consume RuleFlagId_t::NearZeroGrowth / PopulationBoom (emitted by the growth
    // rating's extreme levels in config) once their gameplay rules are defined.
    const double growthRate =
        ResolveStatModifiers(FilterBaseLevelByStatId(rBaseEffects, StatId_t::GrowthRate), 100.0).total;

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
    const double required =
        static_cast<double>(baseSize) * static_cast<double>(rConfig.nutrientsPerPop) / multiplier;
    if (required >= static_cast<double>(std::numeric_limits<int>::max()))
    {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(required);
}

} // namespace ac
