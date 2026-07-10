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
    // TODO: consume RuleFlagId::NearZeroGrowth / PopulationBoom (emitted by the growth
    // rating's extreme levels in config) once their gameplay rules are defined.
    const double growthRate =
        ResolveStatModifiers(FilterFlatByStatId(rBaseEffects, StatId::GrowthRate), 100.0).total;

    // GrowthRate ≤ 0 is not a defined nutrient-threshold rule (SMAC uses the
    // NearZeroGrowth flag at the extreme instead of a zero/negative multiplier).
    // Do not silently treat it as normal growth — block threshold-based growth.
    if (growthRate <= 0.0)
    {
        return std::numeric_limits<int>::max();
    }

    const double multiplier = growthRate / 100.0;
    return static_cast<int>(baseSize * rConfig.nutrientsPerPop / multiplier);
}

} // namespace ac
