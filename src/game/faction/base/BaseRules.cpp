#include "game/faction/base/BaseRules.h"

#include "game/GameDataContext.h"

namespace ac
{

BaseRules_t MakeBaseRules(const GameDataContext& rData)
{
    return BaseRules_t{
        .rBuildings = *rData.buildingRegistry,
        .rStockpiles = *rData.stockpileRegistry,
        .rSocialRatings = *rData.socialRatingRegistry,
        .rPopTypes = *rData.popTypeRegistry,
        .rPopTypeAvailability = *rData.popTypeAvailabilityCalculator,
        .rGrowth = *rData.growthConfig,
        .rProduction = *rData.productionConfig,
        .rHurry = *rData.hurryProductionCalculator,
        .rScrap = *rData.scrapRefundCalculator,
        .rComposition = *rData.popCompositionCalculator,
        .rDrones = *rData.droneCalculator,
        .rEcoDamage = *rData.ecoDamageCalculator,
        .rAtrocities = *rData.atrocitiesConfig,
    };
}

} // namespace ac
