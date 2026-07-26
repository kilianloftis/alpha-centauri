#include "game/GameDataContext.h"

#include "game/EffectReferenceValidator.h"
#include "game/RequiredTechValidator.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/faction/FactionRegistry.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/LandmarkConfig.h"
#include "game/map/LandmarkConfigParser.h"
#include "game/map/WorldGenDecorationConfigParser.h"
#include "game/map/WorldGenPresetRegistry.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitSlotRegistry.h"
#include "game/effects/TileYieldRulesConfigParser.h"
#include "lib/LuaRuntime.h"

#include <stdexcept>

namespace ac
{

GameDataContext::GameDataContext() = default;
GameDataContext::~GameDataContext() = default;

void LoadGameData(GameDataContext& rData, const GameDataPaths& rPaths)
{
    // --- Reference-target registries first (cited by later configs / validators) ---
    rData.techRegistry = std::make_unique<TechRegistry>();
    rData.techRegistry->Load(rPaths.techs);

    rData.improvementRegistry = std::make_unique<ImprovementRegistry>();
    rData.improvementRegistry->Load(rPaths.improvements);

    // unitFilter HasComponent and component required_tech need this present before
    // ValidateEffectReferences / ValidateRequiredTechReferences below.
    rData.unitComponentRegistry = std::make_unique<UnitComponentRegistry>();
    rData.unitComponentRegistry->Load(rPaths.unitComponents);

    rData.unitSlotRegistry = std::make_unique<UnitSlotRegistry>();
    rData.unitSlotRegistry->Load(rPaths.unitSlots);

    // --- Effect-declaring (and required_tech-bearing) registries ---
    rData.popTypeRegistry = std::make_unique<PopTypeRegistry>();
    rData.popTypeRegistry->Load(rPaths.popTypes);
    rData.popTypeAvailabilityCalculator =
        std::make_unique<PopTypeAvailabilityCalculator>(*rData.popTypeRegistry);

    rData.buildingRegistry = std::make_unique<BuildingRegistry>();
    rData.buildingRegistry->Load(rPaths.buildings);

    rData.socialPolicyRegistry = std::make_unique<SocialPolicyRegistry>();
    rData.socialPolicyRegistry->Load(rPaths.socialPolicies);

    rData.socialRatingRegistry = std::make_unique<SocialRatingRegistry>();
    rData.socialRatingRegistry->Load(rPaths.socialRatings);

    rData.factionRegistry = std::make_unique<FactionRegistry>();
    rData.factionRegistry->Load(rPaths.factions);

    rData.worldGenPresetRegistry = std::make_unique<WorldGenPresetRegistry>();
    rData.worldGenPresetRegistry->Load(rPaths.worldGenPresets);

    WorldGenDecorationConfigParser decorationParser;
    rData.worldGenDecorationConfig = std::make_unique<WorldGenDecorationConfig_t>(
        decorationParser.ParseConfig(rPaths.worldGenDecoration));

    {
        std::vector<std::string> improvementIds;
        for (const ImprovementConfig_t& rConfig : rData.improvementRegistry->GetAll())
        {
            improvementIds.push_back(rConfig.id);
        }
        LandmarkConfigParser landmarkParser;
        rData.worldGenLandmarks =
            landmarkParser.ParseConfig(rPaths.worldGenLandmarks, improvementIds);
    }

    TileYieldRulesConfigParser tileYieldRulesParser;
    rData.tileYieldRules = tileYieldRulesParser.ParseConfig(rPaths.tileYieldRules);

    // Cross-config id checks — only safe once every registry above is loaded.
    ValidateEffectReferences(rData);
    ValidateRequiredTechReferences(rData);

    // --- Formula configs / calculators (depend on registries + LuaRuntime) ---
    rData.luaRuntime = std::make_unique<LuaRuntime>();

    PopCompositionConfigParser compositionParser;
    rData.popCompositionConfig = std::make_unique<PopCompositionConfig_t>(
        compositionParser.ParseConfig(rPaths.popComposition, *rData.luaRuntime));
    {
        const PopCompositionConfig_t& rComposition = *rData.popCompositionConfig;
        if (!rData.popTypeRegistry->Find(rComposition.droneTypeId))
        {
            throw std::runtime_error(
                "pop composition drone_type '" + rComposition.droneTypeId
                + "' is not a known pop type");
        }
        if (!rData.popTypeRegistry->Find(rComposition.talentTypeId))
        {
            throw std::runtime_error(
                "pop composition talent_type '" + rComposition.talentTypeId
                + "' is not a known pop type");
        }
    }
    rData.popCompositionCalculator = std::make_unique<PopCompositionCalculator>(
        *rData.popCompositionConfig, *rData.luaRuntime);

    GrowthConfigParser growthParser;
    rData.growthConfig =
        std::make_unique<GrowthConfig_t>(growthParser.ParseConfig(rPaths.popGrowth));

    TechCostConfigParser techCostParser;
    rData.techCostConfig = std::make_unique<TechCostConfig_t>(
        techCostParser.ParseConfig(rPaths.techCost, *rData.luaRuntime));
    rData.techCostCalculator =
        std::make_unique<TechCostCalculator>(*rData.techCostConfig, *rData.luaRuntime);
}

} // namespace ac
