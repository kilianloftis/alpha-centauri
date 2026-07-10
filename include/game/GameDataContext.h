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
class SocialRatingRegistry;
class FactionRegistry;
struct PopCompositionConfig;
class PopCompositionCalculator;
struct GrowthConfig_t;
class LuaRuntime;
struct TechCostConfig;
class TechCostCalculator;
class PopTypeAvailabilityCalculator;
class ImprovementRegistry;

// Owns the definition data loaded once at startup (registries and config structs, all
// reconstructible from config files) plus the calculators/services built from that data.
// Individual pieces are not mutated after loading, but this is not a blanket immutability
// guarantee: LuaRuntime carries mutable interpreter state used internally by every formula
// evaluation (see code-review-findings.md 3.6). Deliberately excludes anything that reads
// live save-game state — e.g. SecretProjectAvailabilityCalculator lives on GameState instead,
// since it queries the faction vector and would dangle if GameState were ever rebuilt.
struct GameDataContext
{
    GameDataContext();
    ~GameDataContext();

    // --- Registries and config structs (definition data) ---
    std::unique_ptr<BuildingRegistry> buildingRegistry;
    std::unique_ptr<UnitComponentRegistry> unitComponentRegistry;
    std::unique_ptr<UnitSlotRegistry> unitSlotRegistry;
    std::unique_ptr<TechRegistry> techRegistry;
    std::unique_ptr<SocialPolicyRegistry> socialPolicyRegistry;
    std::unique_ptr<SocialRatingRegistry> socialRatingRegistry;
    std::unique_ptr<FactionRegistry> factionRegistry;
    std::unique_ptr<PopTypeRegistry> popTypeRegistry;
    std::unique_ptr<PopCompositionConfig> popCompositionConfig;
    std::unique_ptr<GrowthConfig_t> growthConfig;
    std::unique_ptr<TechCostConfig> techCostConfig;
    std::unique_ptr<ImprovementRegistry> improvementRegistry;

    // --- Calculators / services (built from the data above) ---
    std::unique_ptr<LuaRuntime> luaRuntime;
    std::unique_ptr<PopCompositionCalculator> popCompositionCalculator;
    std::unique_ptr<TechCostCalculator> techCostCalculator;
    std::unique_ptr<PopTypeAvailabilityCalculator> popTypeAvailabilityCalculator;
};

} // namespace ac
