#include "game/base/BaseRuleset.h"

#include "game/GameDataContext.h"

namespace ac
{

BaseRuleset_t MakeBaseRuleset(const GameDataContext& rData)
{
    return BaseRuleset_t{
        .rBuildings = *rData.buildingRegistry,
        .rStockpiles = *rData.stockpileRegistry,
        .rSocialRatingRegistry = *rData.socialRatingRegistry,
        .rPopTypes = *rData.popTypeRegistry,
        .rPopTypeAvailability = *rData.popTypeAvailabilityCalculator,
        .rGrowth = *rData.growthConfig,
        .rProduction = *rData.productionConfig,
        .rHurry = *rData.hurryProductionCalculator,
        .rScrap = *rData.scrapRefundCalculator,
        .rComposition = *rData.popCompositionCalculator,
        .rDrones = *rData.droneCalculator,
        .rEcoDamage = *rData.ecoDamageCalculator,
    };
}

} // namespace ac
