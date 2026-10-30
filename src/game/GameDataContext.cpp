#include "game/GameDataContext.h"

#include "game/EffectReferenceValidator.h"
#include "game/RequiredTechValidator.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/display/BaseSpriteSizesConfig.h"
#include "game/display/BaseSpriteSizesConfigParser.h"
#include "game/display/MapOverlayChannelsConfig.h"
#include "game/display/MapOverlayChannelsConfigParser.h"
#include "game/production/StockpileRegistry.h"
#include "game/faction/FactionRegistry.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapOccupantLoad.h"
#include "game/map/TerrainConfig.h"
#include "game/map/TerrainFeatureValidation.h"
#include "game/map/TerrainOperationRegistry.h"
#include "game/worldgen/LandmarkConfig.h"
#include "game/worldgen/LandmarkConfigParser.h"
#include "game/worldgen/WorldGenDecorationConfigParser.h"
#include "game/worldgen/WorldGenPresetRegistry.h"
#include "game/population/DroneCalculator.h"
#include "game/population/PopCompositionCalculator.h"
#include "game/population/PopTypeAvailabilityCalculator.h"
#include "game/production/ProductionConfigParser.h"
#include "game/economy/CommerceConfigParser.h"
#include "game/diplomacy/DiplomacyConfigParser.h"
#include "game/population/GrowthConfigParser.h"
#include "game/population/PopCompositionConfigParser.h"
#include "game/population/PopTypeRegistry.h"
#include "game/production/HurryProductionCalculator.h"
#include "game/production/ScrapRefundCalculator.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/units/UnitComponentConfigParser.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitSlotRegistry.h"
#include "game/natives/NativeUnitRegistry.h"
#include "game/effects/TileYieldRulesConfigParser.h"
#include "game/map/ElevationRulesConfigParser.h"
#include "game/effects/InteractionGridsConfigParser.h"
#include "game/population/PoliceRulesConfigParser.h"
#include "game/effects/WorldRulesConfigParser.h"
#include "game/combat/MoraleCalculator.h"
#include "game/economy/CommerceCalculator.h"
#include "game/probes/ProbeCalculator.h"
#include "game/diplomacy/DiplomacyCalculator.h"
#include "game/combat/MoraleConfigParser.h"
#include "game/probes/ProbeActionConfigParser.h"
#include "game/base/BaseConquestConfigParser.h"
#include "game/combat/CombatConfigParser.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/atrocities/AtrocityConfigParser.h"
#include "game/council/CouncilRulesConfigParser.h"
#include "game/DifficultyConfigParser.h"
#include "game/natives/NativeLifeLevelConfig.h"
#include "game/ecology/EcoDamageCalculator.h"
#include "game/ecology/EcoDamageConfig.h"
#include "game/world-events/WorldEventConfig.h"
#include "lib/LuaRuntime.h"

#include <stdexcept>
#include <unordered_map>

namespace ac
{

GameDataContext::GameDataContext() = default;
GameDataContext::~GameDataContext() = default;
GameDataContext::GameDataContext(GameDataContext&&) noexcept = default;
GameDataContext& GameDataContext::operator=(GameDataContext&&) noexcept = default;

namespace
{

template <typename TParser>
auto Parse_(const std::string& rPath)
{
    TParser parser;
    return parser.ParseConfig(rPath);
}

template <typename TParser>
auto ParseOwned_(const std::string& rPath)
{
    using Config_t = decltype(Parse_<TParser>(rPath));
    return std::make_unique<Config_t>(Parse_<TParser>(rPath));
}

template <typename TRegistry>
std::unique_ptr<TRegistry> LoadRegistry_(const auto& rPath)
{
    auto pRegistry = std::make_unique<TRegistry>();
    pRegistry->Load(rPath);
    return pRegistry;
}

void ValidateOverlayChannels_(const GameDataContext& rData)
{
    for (const BuildingConfig_t& rBuilding : rData.buildingRegistry->GetAll())
    {
        if (rBuilding.mapOverlayChannel.empty())
        {
            continue;
        }
        if (!rData.mapOverlayChannels->layersByChannel.contains(rBuilding.mapOverlayChannel))
        {
            throw std::runtime_error(
                "Building '" + rBuilding.id + "': unknown map_overlay_channel '"
                + rBuilding.mapOverlayChannel + "'");
        }
    }
}

void ValidateStageBumpBuildings_(const GameDataContext& rData)
{
    for (const std::string& rBumpId : rData.baseSpriteSizes->stageBumpBuildings)
    {
        if (!rData.buildingRegistry->Find(rBumpId))
        {
            throw std::runtime_error(
                "base_sprite_sizes: stage_bump_buildings names unknown building '" + rBumpId
                + "'");
        }
    }
}

// Escape pods are assembled only on a cross-species capture, so their ids are checked at load.
void ValidateEscapePodComponents_(const GameDataContext& rData)
{
    for (const std::string& rComponentId : rData.baseConquestConfig->escapeColonyPod.componentIds)
    {
        if (!rData.unitComponentRegistry->Find(rComponentId))
        {
            throw std::runtime_error(
                "base conquest escape_colony_pod component '" + rComponentId
                + "' is not a known unit component");
        }
    }
}

void ValidateCompositionPopTypes_(const GameDataContext& rData)
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

// Evaluated once at load, so a broken formula fails here and not at the first promotion.
void CheckPromotionFormulaEvaluates_(const GameDataContext& rData)
{
    const std::unordered_map<std::string, double> kSmokeCases[] = {
        {{"attack_strength", 1.0}, {"defense_strength", 1.0}},
        {{"attack_strength", 0.0}, {"defense_strength", 0.0}},
    };
    for (const auto& vars : kSmokeCases)
    {
        (void)rData.luaRuntime->EvalDouble(rData.moraleConfig->promotionSeedFormula, vars);
    }
}

std::vector<LandmarkConfig_t> LoadLandmarks_(const GameDataContext& rData)
{
    std::vector<std::string> occupantIds;
    for (const ImprovementConfig_t& rConfig : rData.improvementRegistry->GetAll())
    {
        occupantIds.push_back(rConfig.id);
    }
    LandmarkConfigParser landmarkParser;
    std::vector<LandmarkConfig_t> landmarks =
        landmarkParser.ParseConfig(rData.paths.worldGenLandmarks, occupantIds);
    for (const WorldGenPresetConfig_t& rPreset : rData.worldGenPresetRegistry->GetAll())
    {
        for (const LandmarkConfig_t& rLandmark : landmarks)
        {
            LandmarkConfigParser::ValidateSculptAgainstPreset(rLandmark, rPreset);
        }
    }
    return landmarks;
}

// Cited by later configs and validators, so they load first.
void LoadReferenceRegistries_(GameDataContext& rData)
{
    const GameDataPaths& rPaths = rData.paths;
    rData.techRegistry = LoadRegistry_<TechRegistry>(rPaths.techs);
    rData.terrainOperationRegistry = std::make_unique<TerrainOperationRegistry>();
    rData.improvementRegistry = std::make_unique<ImprovementRegistry>();
    LoadMapOccupants(rPaths.improvements, rPaths.terrain, *rData.improvementRegistry,
                     *rData.terrainOperationRegistry);
    // HasComponent conditions and component required_tech need this before the reference
    // validators.
    rData.unitComponentRegistry = LoadRegistry_<UnitComponentRegistry>(rPaths.unitComponents);
    ValidateComponentChassisRequirements(*rData.unitComponentRegistry);
    rData.unitSlotRegistry = LoadRegistry_<UnitSlotRegistry>(rPaths.unitSlots);
    rData.nativeUnitRegistry = LoadRegistry_<NativeUnitRegistry>(rPaths.nativeUnits);
}

// Registries that declare effects or carry required_tech.
void LoadEffectRegistries_(GameDataContext& rData)
{
    const GameDataPaths& rPaths = rData.paths;
    rData.popTypeRegistry = LoadRegistry_<PopTypeRegistry>(rPaths.popTypes);
    rData.popTypeAvailabilityCalculator =
        std::make_unique<PopTypeAvailabilityCalculator>(*rData.popTypeRegistry);
    rData.buildingRegistry = LoadRegistry_<BuildingRegistry>(rPaths.buildings);
    rData.mapOverlayChannels =
        ParseOwned_<MapOverlayChannelsConfigParser>(rPaths.mapOverlayChannels);
    ValidateOverlayChannels_(rData);
    rData.baseSpriteSizes = ParseOwned_<BaseSpriteSizesConfigParser>(rPaths.baseSpriteSizes);
    ValidateStageBumpBuildings_(rData);
    rData.stockpileRegistry = LoadRegistry_<StockpileRegistry>(rPaths.stockpiles);
    rData.socialPolicyRegistry = LoadRegistry_<SocialPolicyRegistry>(rPaths.socialPolicies);
    rData.socialRatingRegistry = LoadRegistry_<SocialRatingRegistry>(rPaths.socialRatings);
    rData.factionRegistry = LoadRegistry_<FactionRegistry>(rPaths.factions);
}

void LoadWorldGenData_(GameDataContext& rData)
{
    const GameDataPaths& rPaths = rData.paths;
    rData.elevationRules = Parse_<ElevationRulesConfigParser>(rPaths.elevationRules);
    rData.worldGenPresetRegistry = LoadRegistry_<WorldGenPresetRegistry>(rPaths.worldGenPresets);
    for (const WorldGenPresetConfig_t& rPreset : rData.worldGenPresetRegistry->GetAll())
    {
        WorldGenPresetConfigParser::ValidateAgainstMapRules(rPreset, rData.elevationRules);
    }
    rData.worldGenDecorationConfig =
        ParseOwned_<WorldGenDecorationConfigParser>(rPaths.worldGenDecoration);
    rData.worldGenLandmarks = LoadLandmarks_(rData);
}

void LoadRuleConfigs_(GameDataContext& rData)
{
    const GameDataPaths& rPaths = rData.paths;
    rData.tileYieldRules = Parse_<TileYieldRulesConfigParser>(rPaths.tileYieldRules);
    rData.interactionGrids = Parse_<InteractionGridsConfigParser>(rPaths.interactionGrids);
    rData.policeRules = Parse_<PoliceRulesConfigParser>(rPaths.policeRules);
    rData.worldRules = Parse_<WorldRulesConfigParser>(rPaths.worldRules);
    rData.moraleConfig = ParseOwned_<MoraleConfigParser>(rPaths.moraleLevels);
    rData.probeActionsConfig = ParseOwned_<ProbeActionConfigParser>(rPaths.probeActions);
    rData.baseConquestConfig = ParseOwned_<BaseConquestConfigParser>(rPaths.baseConquest);
    rData.combatConfig = ParseOwned_<CombatConfigParser>(rPaths.combat);
    ValidateEscapePodComponents_(rData);
    rData.councilProposalRegistry = LoadRegistry_<CouncilProposalRegistry>(rPaths.councilProposals);
    rData.councilRules = ParseOwned_<CouncilRulesConfigParser>(rPaths.councilRules);
    rData.atrocitiesConfig = ParseOwned_<AtrocityConfigParser>(rPaths.atrocities);
    // Declares effects (prototype starting XP), so it loads before ValidateEffectReferences.
    rData.productionConfig = ParseOwned_<ProductionConfigParser>(rPaths.production);
    rData.commerceConfig = ParseOwned_<CommerceConfigParser>(rPaths.commerce);
    rData.diplomacyConfig = ParseOwned_<DiplomacyConfigParser>(rPaths.diplomacy);
    rData.difficultyConfig = ParseOwned_<DifficultyConfigParser>(rPaths.difficulty);
    rData.nativeLifeLevelConfig = ParseOwned_<NativeLifeLevelConfigParser>(rPaths.nativeLifeLevels);
    rData.worldEventsConfig = ParseOwned_<WorldEventsConfigParser>(rPaths.worldEvents);
    // Drone clamps and mood thresholds, loaded before the Lua calculators that use them.
    rData.popCompositionConfig = ParseOwned_<PopCompositionConfigParser>(rPaths.popComposition);
    ValidateCompositionPopTypes_(rData);

    // Created here so eco_damage.lua loads before ValidateEffectReferences walks eco_damage.json.
    rData.luaRuntime = std::make_unique<LuaRuntime>();
    EcoDamageConfigParser ecoDamageParser;
    rData.ecoDamageConfig = std::make_unique<EcoDamageConfig_t>(ecoDamageParser.ParseConfig(
        rPaths.ecoDamage, rPaths.ecoDamageFormula, *rData.luaRuntime));
}

// Formula configs and calculators, which need the registries and the Lua runtime.
void LoadFormulasAndCalculators_(GameDataContext& rData)
{
    const GameDataPaths& rPaths = rData.paths;
    LuaRuntime& rLua = *rData.luaRuntime;
    CheckPromotionFormulaEvaluates_(rData);
    rData.droneCalculator = std::make_unique<DroneCalculator>(*rData.popCompositionConfig, rLua);
    rData.popCompositionCalculator = std::make_unique<PopCompositionCalculator>(
        *rData.popCompositionConfig, *rData.popTypeRegistry);
    rData.growthConfig = ParseOwned_<GrowthConfigParser>(rPaths.popGrowth);

    TechCostConfigParser techCostParser;
    rData.techCostConfig =
        std::make_unique<TechCostConfig_t>(techCostParser.ParseConfig(rPaths.techCost, rLua));
    rData.techCostCalculator = std::make_unique<TechCostCalculator>(*rData.techCostConfig, rLua);
    rData.ecoDamageCalculator = std::make_unique<EcoDamageCalculator>(*rData.ecoDamageConfig, rLua);
    rData.hurryProductionCalculator =
        std::make_unique<HurryProductionCalculator>(*rData.productionConfig, rLua);
    rData.scrapRefundCalculator =
        std::make_unique<ScrapRefundCalculator>(*rData.productionConfig, rLua);
    rData.moraleCalculator = std::make_unique<MoraleCalculator>(*rData.moraleConfig, rLua);
    rData.commerceCalculator = std::make_unique<CommerceCalculator>(*rData.commerceConfig, rLua);
    rData.probeCalculator = std::make_unique<ProbeCalculator>(rLua);
    rData.diplomacyCalculator = std::make_unique<DiplomacyCalculator>(*rData.diplomacyConfig);
}

} // namespace

void ThrowIfIncomplete(const GameDataContext& rData)
{
    // Consumers dereference these without checking. Named one by one, so a modder can act on the
    // message.
    const std::pair<const void*, const char*> required[] = {
        {rData.buildingRegistry.get(), "buildingRegistry"},
        {rData.mapOverlayChannels.get(), "mapOverlayChannels"},
        {rData.baseSpriteSizes.get(), "baseSpriteSizes"},
        {rData.stockpileRegistry.get(), "stockpileRegistry"},
        {rData.unitComponentRegistry.get(), "unitComponentRegistry"},
        {rData.unitSlotRegistry.get(), "unitSlotRegistry"},
        {rData.nativeUnitRegistry.get(), "nativeUnitRegistry"},
        {rData.techRegistry.get(), "techRegistry"},
        {rData.socialPolicyRegistry.get(), "socialPolicyRegistry"},
        {rData.socialRatingRegistry.get(), "socialRatingRegistry"},
        {rData.factionRegistry.get(), "factionRegistry"},
        {rData.popTypeRegistry.get(), "popTypeRegistry"},
        {rData.popCompositionConfig.get(), "popCompositionConfig"},
        {rData.growthConfig.get(), "growthConfig"},
        {rData.productionConfig.get(), "productionConfig"},
        {rData.commerceConfig.get(), "commerceConfig"},
        {rData.diplomacyConfig.get(), "diplomacyConfig"},
        {rData.techCostConfig.get(), "techCostConfig"},
        {rData.improvementRegistry.get(), "improvementRegistry"},
        {rData.terrainOperationRegistry.get(), "terrainOperationRegistry"},
        {rData.worldGenPresetRegistry.get(), "worldGenPresetRegistry"},
        {rData.worldGenDecorationConfig.get(), "worldGenDecorationConfig"},
        {rData.moraleConfig.get(), "moraleConfig"},
        {rData.probeActionsConfig.get(), "probeActionsConfig"},
        {rData.baseConquestConfig.get(), "baseConquestConfig"},
        {rData.combatConfig.get(), "combatConfig"},
        {rData.councilProposalRegistry.get(), "councilProposalRegistry"},
        {rData.councilRules.get(), "councilRules"},
        {rData.difficultyConfig.get(), "difficultyConfig"},
        {rData.atrocitiesConfig.get(), "atrocitiesConfig"},
        {rData.nativeLifeLevelConfig.get(), "nativeLifeLevelConfig"},
        {rData.worldEventsConfig.get(), "worldEventsConfig"},
        {rData.ecoDamageConfig.get(), "ecoDamageConfig"},
        {rData.luaRuntime.get(), "luaRuntime"},
        {rData.droneCalculator.get(), "droneCalculator"},
        {rData.popCompositionCalculator.get(), "popCompositionCalculator"},
        {rData.techCostCalculator.get(), "techCostCalculator"},
        {rData.ecoDamageCalculator.get(), "ecoDamageCalculator"},
        {rData.hurryProductionCalculator.get(), "hurryProductionCalculator"},
        {rData.scrapRefundCalculator.get(), "scrapRefundCalculator"},
        {rData.popTypeAvailabilityCalculator.get(), "popTypeAvailabilityCalculator"},
        {rData.moraleCalculator.get(), "moraleCalculator"},
        {rData.commerceCalculator.get(), "commerceCalculator"},
        {rData.probeCalculator.get(), "probeCalculator"},
        {rData.diplomacyCalculator.get(), "diplomacyCalculator"},
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
    LoadReferenceRegistries_(rData);
    LoadEffectRegistries_(rData);
    LoadWorldGenData_(rData);
    LoadRuleConfigs_(rData);

    // Cross-config id checks — only safe once every registry above is loaded.
    ValidateTerrainFeatures(*rData.improvementRegistry);
    ValidateEffectReferences(rData);
    ValidateRequiredTechReferences(rData);

    LoadFormulasAndCalculators_(rData);
    ThrowIfIncomplete(rData);
    return rData;
}

} // namespace ac
