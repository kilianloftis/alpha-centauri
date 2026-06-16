#pragma once

#include <memory>

namespace ac
{

class BuildingRegistry;
class PopTypeRegistry;
class TechRegistry;
class SocialPolicyRegistry;
struct PopCompositionConfig;
class PopCompositionCalculator;
struct GrowthConfig;
class GrowthCalculator;
class LuaRuntime;
struct TechCostConfig;
class TechCostCalculator;

// Holds all immutable definition data loaded once at startup.
// Never serialised — always reconstructible from config files.
struct GameDataContext
{
    GameDataContext();
    ~GameDataContext();

    std::unique_ptr<BuildingRegistry> buildingRegistry;
    std::unique_ptr<TechRegistry> techRegistry;
    std::unique_ptr<SocialPolicyRegistry> socialPolicyRegistry;
    std::unique_ptr<PopTypeRegistry> popTypeRegistry;
    std::unique_ptr<PopCompositionConfig> popCompositionConfig;
    std::unique_ptr<PopCompositionCalculator> popCompositionCalculator;
    std::unique_ptr<GrowthConfig> growthConfig;
    std::unique_ptr<GrowthCalculator> growthCalculator;
    std::unique_ptr<LuaRuntime> luaRuntime;
    std::unique_ptr<TechCostConfig> techCostConfig;
    std::unique_ptr<TechCostCalculator> techCostCalculator;
};

} // namespace ac
