#pragma once

namespace ac
{

class BuildingRegistry;
class StockpileRegistry;
class SocialRatingRegistry;
class PopTypeRegistry;
class PopTypeAvailabilityCalculator;
struct GrowthConfig_t;
struct ProductionConfig_t;
class HurryProductionCalculator;
class ScrapRefundCalculator;
class PopCompositionCalculator;
class DroneCalculator;
class EcoDamageCalculator;
struct AtrocitiesConfig_t;
struct GameDataContext;

// The ruleset pieces a base reads. BaseManager copies each reference into the member or part
// that uses it and never keeps the bundle, so a caller may pass a temporary.
struct BaseRules_t
{
    const BuildingRegistry& rBuildings;
    const StockpileRegistry& rStockpiles;
    const SocialRatingRegistry& rSocialRatings;
    const PopTypeRegistry& rPopTypes;
    const PopTypeAvailabilityCalculator& rPopTypeAvailability;
    const GrowthConfig_t& rGrowth;
    const ProductionConfig_t& rProduction;
    const HurryProductionCalculator& rHurry;
    const ScrapRefundCalculator& rScrap;
    PopCompositionCalculator& rComposition;
    const DroneCalculator& rDrones;
    const EcoDamageCalculator& rEcoDamage;
    const AtrocitiesConfig_t& rAtrocities;
};

BaseRules_t MakeBaseRules(const GameDataContext& rData);

} // namespace ac
