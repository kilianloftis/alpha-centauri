#pragma once

#include <memory>

namespace ac
{

class BuildingRegistry;
class UnitComponentRegistry;
class UnitSlotRegistry;
class PopTypeRegistry;
class TechRegistry;
class SocialPolicyRegistry;
struct PopCompositionConfig;
class PopCompositionCalculator;
struct GrowthConfig_t;
class GrowthCalculator;
class LuaRuntime;
struct TechCostConfig;
class TechCostCalculator;
struct ProductionCostConfig_t;
class ProductionCostCalculator;
class PopTypeAvailabilityCalculator;
class SecretProjectAvailabilityCalculator;
class ImprovementRegistry;

// Holds all immutable definition data loaded once at startup.
// Never serialised — always reconstructible from config files.
struct GameDataContext
{
    GameDataContext();
    ~GameDataContext();

    std::unique_ptr<BuildingRegistry> buildingRegistry;
    std::unique_ptr<UnitComponentRegistry> unitComponentRegistry;
    std::unique_ptr<UnitSlotRegistry> unitSlotRegistry;
    std::unique_ptr<TechRegistry> techRegistry;
    std::unique_ptr<SocialPolicyRegistry> socialPolicyRegistry;
    std::unique_ptr<PopTypeRegistry> popTypeRegistry;
    std::unique_ptr<PopCompositionConfig> popCompositionConfig;
    std::unique_ptr<PopCompositionCalculator> popCompositionCalculator;
    std::unique_ptr<GrowthConfig_t> growthConfig;
    std::unique_ptr<GrowthCalculator> growthCalculator;
    std::unique_ptr<LuaRuntime> luaRuntime;
    std::unique_ptr<TechCostConfig> techCostConfig;
    std::unique_ptr<TechCostCalculator> techCostCalculator;
    std::unique_ptr<ProductionCostConfig_t> productionCostConfig;
    std::unique_ptr<ProductionCostCalculator> productionCostCalculator;
    std::unique_ptr<PopTypeAvailabilityCalculator> popTypeAvailabilityCalculator;
    std::unique_ptr<SecretProjectAvailabilityCalculator> secretProjectAvailabilityCalculator;
    std::unique_ptr<ImprovementRegistry> improvementRegistry;
};

} // namespace ac
