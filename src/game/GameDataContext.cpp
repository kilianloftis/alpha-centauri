#include "game/GameDataContext.h"

#include "game/EffectReferenceValidator.h"
#include "game/RequiredTechValidator.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/stockpiles/StockpileRegistry.h"
#include "game/faction/FactionRegistry.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/TerrainFeatureValidation.h"
#include "game/map/LandmarkConfig.h"
#include "game/map/LandmarkConfigParser.h"
#include "game/map/WorldGenDecorationConfigParser.h"
#include "game/map/WorldGenPresetRegistry.h"
#include "game/population/calculators/DroneCalculator.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/faction/base/production/ProductionConfigParser.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/faction/base/production/HurryProductionCalculator.h"
#include "game/faction/base/production/ScrapRefundCalculator.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitSlotRegistry.h"
#include "game/effects/TileYieldRulesConfigParser.h"
#include "game/effects/PoliceRulesConfigParser.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/MoraleConfigParser.h"
#include "game/units/ProbeActionConfigParser.h"
#include "game/units/BaseConquestConfigParser.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRulesConfigParser.h"
#include "game/DifficultyConfigParser.h"
#include "lib/LuaRuntime.h"

#include <stdexcept>

namespace ac
{

GameDataContext::GameDataContext() = default;
GameDataContext::~GameDataContext() = default;
GameDataContext::GameDataContext(GameDataContext&&) noexcept = default;
GameDataContext& GameDataContext::operator=(GameDataContext&&) noexcept = default;

void ThrowIfIncomplete(const GameDataContext& rData)
{
    // Consumers take these as references and dereference without checking, so an incomplete
    // context must never escape the loader. Named individually: "something is null" is not a
    // diagnostic a modder can act on.
    const std::pair<const void*, const char*> required[] = {
        {rData.buildingRegistry.get(), "buildingRegistry"},
        {rData.stockpileRegistry.get(), "stockpileRegistry"},
        {rData.unitComponentRegistry.get(), "unitComponentRegistry"},
        {rData.unitSlotRegistry.get(), "unitSlotRegistry"},
        {rData.techRegistry.get(), "techRegistry"},
        {rData.socialPolicyRegistry.get(), "socialPolicyRegistry"},
        {rData.socialRatingRegistry.get(), "socialRatingRegistry"},
        {rData.factionRegistry.get(), "factionRegistry"},
        {rData.popTypeRegistry.get(), "popTypeRegistry"},
        {rData.popCompositionConfig.get(), "popCompositionConfig"},
        {rData.growthConfig.get(), "growthConfig"},
        {rData.productionConfig.get(), "productionConfig"},
        {rData.techCostConfig.get(), "techCostConfig"},
        {rData.improvementRegistry.get(), "improvementRegistry"},
        {rData.worldGenPresetRegistry.get(), "worldGenPresetRegistry"},
        {rData.worldGenDecorationConfig.get(), "worldGenDecorationConfig"},
        {rData.moraleConfig.get(), "moraleConfig"},
        {rData.probeActionsConfig.get(), "probeActionsConfig"},
        {rData.baseConquestConfig.get(), "baseConquestConfig"},
        {rData.councilProposalRegistry.get(), "councilProposalRegistry"},
        {rData.councilRules.get(), "councilRules"},
        {rData.difficultyConfig.get(), "difficultyConfig"},
        {rData.luaRuntime.get(), "luaRuntime"},
        {rData.droneCalculator.get(), "droneCalculator"},
        {rData.popCompositionCalculator.get(), "popCompositionCalculator"},
        {rData.techCostCalculator.get(), "techCostCalculator"},
        {rData.hurryProductionCalculator.get(), "hurryProductionCalculator"},
        {rData.scrapRefundCalculator.get(), "scrapRefundCalculator"},
        {rData.popTypeAvailabilityCalculator.get(), "popTypeAvailabilityCalculator"},
        {rData.moraleCalculator.get(), "moraleCalculator"},
    };
    for (const auto& [pMember, pName] : required)
    {
        if (!pMember)
        {
            throw std::runtime_error(
                std::string("GameDataContext is incomplete: '") + pName + "' was not loaded");
        }
    }
}

GameDataContext LoadGameData(const GameDataPaths& rPaths)
{
    GameDataContext rData;
    rData.paths = rPaths;

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

    rData.stockpileRegistry = std::make_unique<StockpileRegistry>();
    rData.stockpileRegistry->Load(rPaths.stockpiles);

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

    PoliceRulesConfigParser policeRulesParser;
    rData.policeRules = policeRulesParser.ParseConfig(rPaths.policeRules);

    MoraleConfigParser moraleParser;
    rData.moraleConfig =
        std::make_unique<MoraleConfig_t>(moraleParser.ParseConfig(rPaths.moraleLevels));

    ProbeActionConfigParser probeParser;
    rData.probeActionsConfig =
        std::make_unique<ProbeActionsConfig_t>(probeParser.ParseConfig(rPaths.probeActions));

    BaseConquestConfigParser conquestParser;
    rData.baseConquestConfig =
        std::make_unique<BaseConquestConfig_t>(conquestParser.ParseConfig(rPaths.baseConquest));
    // Escape-pod components are only assembled on a cross-species capture, which can be hours
    // into a session. Validating the ids here turns a typo in base_conquest.json into a startup
    // error naming the file, instead of a mid-game failure at the one moment the rule fires.
    for (const std::string& rComponentId : rData.baseConquestConfig->escapeColonyPod.componentIds)
    {
        if (!rData.unitComponentRegistry->Find(rComponentId))
        {
            throw std::runtime_error(
                "base conquest escape_colony_pod component '" + rComponentId
                + "' is not a known unit component");
        }
    }

    rData.councilProposalRegistry = std::make_unique<CouncilProposalRegistry>();
    rData.councilProposalRegistry->Load(rPaths.councilProposals);

    CouncilRulesConfigParser councilRulesParser;
    rData.councilRules =
        std::make_unique<CouncilRulesConfig_t>(councilRulesParser.ParseConfig(rPaths.councilRules));

    // Effect-declaring, not a Lua formula: prototype starting-XP (and any other universal
    // production rules) live here, so ValidateEffectReferences must see this before it runs.
    ProductionConfigParser productionParser;
    rData.productionConfig =
        std::make_unique<ProductionConfig_t>(productionParser.ParseConfig(rPaths.production));

    DifficultyConfigParser difficultyParser;
    rData.difficultyConfig =
        std::make_unique<DifficultyConfig_t>(difficultyParser.ParseConfig(rPaths.difficulty));

    // Cross-config id checks — only safe once every registry above is loaded.
    ValidateTerrainFeatures(*rData.improvementRegistry);
    ValidateEffectReferences(rData);
    ValidateRequiredTechReferences(rData);

    // --- Formula configs / calculators (depend on registries + LuaRuntime) ---
    rData.luaRuntime = std::make_unique<LuaRuntime>();

    PopCompositionConfigParser compositionParser;
    rData.popCompositionConfig = std::make_unique<PopCompositionConfig_t>(
        compositionParser.ParseConfig(rPaths.popComposition));
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
    rData.droneCalculator =
        std::make_unique<DroneCalculator>(*rData.popCompositionConfig, *rData.luaRuntime);
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

    rData.hurryProductionCalculator = std::make_unique<HurryProductionCalculator>(
        *rData.productionConfig, *rData.luaRuntime);
    rData.scrapRefundCalculator =
        std::make_unique<ScrapRefundCalculator>(*rData.productionConfig, *rData.luaRuntime);

    rData.moraleCalculator = std::make_unique<MoraleCalculator>(*rData.moraleConfig);

    ThrowIfIncomplete(rData);
    return rData;
}

} // namespace ac
