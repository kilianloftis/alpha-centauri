#include "game/population/calculators/GrowthCalculator.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "lib/effects/ActiveEffect.h"
#include "lib/effects/EffectEnums.h"

namespace ac
{

int GrowthCalculator::ComputeNutrientsRequired(const GrowthConfig_t& rConfig, int baseSize, const std::vector<ActiveEffect_t>& activeEffects)
{
    const double growthRate =
        ResolveStatModifiers(FilterFlatByStatId(activeEffects, StatId::GrowthRate), 100.0).total;
    const double multiplier = (growthRate > 0.0) ? growthRate / 100.0 : 1.0;
    return static_cast<int>(baseSize * rConfig.nutrientsPerPop / multiplier);
}

} // namespace ac
