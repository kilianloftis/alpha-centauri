#include "GameFixtures.h"

#include "game/DifficultyConfigParser.h"
#include "game/atrocities/AtrocityConfigParser.h"
#include "game/base/BaseConquestConfig.h"
#include "game/base/BaseManager.h"
#include "game/base/BaseRuleset.h"
#include "game/buildings/BuildingManager.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/combat/AttackRules.h"
#include "game/combat/CollateralMutations.h"
#include "game/combat/CombatConfigParser.h"
#include "game/combat/CombatResolver.h"
#include "game/combat/MoraleCalculator.h"
#include "game/combat/MoraleConfig.h"
#include "game/combat/MoraleConfigParser.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRulesConfigParser.h"
#include "game/diplomacy/DiplomacyCalculator.h"
#include "game/diplomacy/DiplomacyConfigParser.h"
#include "game/display/BaseSpriteSizesConfig.h"
#include "game/display/BaseSpriteSizesConfigParser.h"
#include "game/display/MapOverlayChannelsConfig.h"
#include "game/display/MapOverlayChannelsConfigParser.h"
#include "game/ecology/EcoDamageCalculator.h"
#include "game/ecology/EcoDamageConfig.h"
#include "game/economy/CommerceCalculator.h"
#include "game/economy/CommerceConfigParser.h"
#include "game/economy/ResourceManager.h"
#include "game/effects/InteractionGridsConfigParser.h"
#include "game/effects/TileEffectsContext.h"
#include "game/effects/TileYieldRulesConfigParser.h"
#include "game/effects/WorldRulesConfigParser.h"
#include "game/faction/Faction.h"
#include "game/map/MapRulesConfigParser.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapOccupantLoad.h"
#include "game/map/TerrainConfig.h"
#include "game/map/TerrainOperationRegistry.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/movement/MovementRules.h"
#include "game/natives/NativeLifeLevelConfig.h"
#include "game/natives/NativeUnitConfig.h"
#include "game/natives/NativeUnitRegistry.h"
#include "game/population/DroneCalculator.h"
#include "game/population/GrowthConfigParser.h"
#include "game/population/PoliceRulesConfigParser.h"
#include "game/population/PopCompositionCalculator.h"
#include "game/population/PopCompositionConfigParser.h"
#include "game/population/PopTypeAvailabilityCalculator.h"
#include "game/population/PopTypeRegistry.h"
#include "game/probes/ProbeActionConfigParser.h"
#include "game/probes/ProbeCalculator.h"
#include "game/production/HurryProductionCalculator.h"
#include "game/production/ProductionConfigParser.h"
#include "game/production/ScrapRefundCalculator.h"
#include "game/production/StockpileRegistry.h"
#include "game/research/ResearchManager.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "game/social-engineering/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/units/Unit.h"
#include "game/units/UnitAttackExecutor.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitManager.h"
#include "game/units/UnitSlotConfig.h"
#include "game/world-events/WorldEventConfig.h"
#include "game/worldgen/WorldGenDecorationConfig.h"
#include "game/worldgen/WorldGenDecorationConfigParser.h"
#include "lib/LuaRuntime.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <unordered_map>

namespace actest
{

namespace
{

template <typename T>
T& CreateInContext(std::unique_ptr<T>& pSlot)
{
    pSlot = std::make_unique<T>();
    return *pSlot;
}

} // namespace

void SetBaseConquestStat(ac::BaseConquestConfig_t& rConfig, ac::StatId_t stat, double amount)
{
    for (ac::EffectConfig_t& rEffect : rConfig.effects)
    {
        auto* pMod = std::get_if<ac::StatModifierEffect_t>(&rEffect.effect);
        if (pMod && pMod->stat == stat)
        {
            pMod->amount = amount;
            return;
        }
    }
    ac::EffectConfig_t effect;
    effect.scope = ac::EffectScope_t::FactionGlobal;
    ac::StatModifierEffect_t modifier;
    modifier.stat = stat;
    modifier.amount = amount;
    modifier.op = ac::ModifierOp_t::Add;
    effect.effect = modifier;
    rConfig.effects.push_back(effect);
}

SessionInputs::SessionInputs(const OccupantFiles_t& rOccupants,
                             std::vector<ac::EffectConfig_t> worldRules)
    : improvements(CreateInContext(dataContext.improvementRegistry))
    , unitComponents(CreateInContext(dataContext.unitComponentRegistry))
{
    // As LoadGameData leaves it: the preset's elevation range belongs to the map.
    dataContext.mapRules =
        ac::MapRulesConfigParser{}.ParseConfig(FixturePath("map_rules.json"));
    // The same call production uses, so the fixture cannot drift from it or skip the
    // improvement-shadows-operation check that only this path performs.
    dataContext.terrainOperationRegistry = std::make_unique<ac::TerrainOperationRegistry>();
    ac::LoadMapOccupants(FixturePath(rOccupants.improvements), FixturePath(rOccupants.terrain),
                         improvements, *dataContext.terrainOperationRegistry);
    unitComponents.Load(FixturePath("unit_components.json"));
    // Before any TileEffectsContext: it holds the yield rules by reference and stamps
    // them into every per-tile resolve (ElevationEnergy reads the step from here).
    dataContext.tileYieldRules = ac::TileYieldRulesConfigParser{}.ParseConfig(
        FixturePath("tile_yield_rules.json"));
    dataContext.interactionGrids = ac::InteractionGridsConfigParser{}.ParseConfig(
        FixturePath("interaction_grids.json"));
    // Climate re-runs read the rainfall knobs.
    dataContext.worldGenDecorationConfig = std::make_unique<ac::WorldGenDecorationConfig_t>(
        ac::WorldGenDecorationConfigParser{}.ParseConfig(
            FixturePath("worldGen/decoration.json")));
    dataContext.worldRules = std::move(worldRules);
    dataContext.luaRuntime = std::make_unique<ac::LuaRuntime>();
    // Same morale table Engine loads, and the one calculator built from it.
    dataContext.moraleConfig = std::make_unique<ac::MoraleConfig_t>(
        ac::MoraleConfigParser{}.ParseConfig(FixturePath("morale_levels.json")));
    dataContext.moraleCalculator = std::make_unique<ac::MoraleCalculator>(
        *dataContext.moraleConfig, *dataContext.luaRuntime);

    dataContext.buildingRegistry = std::make_unique<ac::BuildingRegistry>();
    dataContext.buildingRegistry->Load(FixturePath("buildings.json"));
    dataContext.mapOverlayChannels = std::make_unique<ac::MapOverlayChannelsConfig_t>(
        ac::MapOverlayChannelsConfigParser{}.ParseConfig(
            FixturePath("map_overlay_channels.json")));
    dataContext.baseSpriteSizes = std::make_unique<ac::BaseSpriteSizesConfig_t>(
        ac::BaseSpriteSizesConfigParser{}.ParseConfig(FixturePath("base_sprite_sizes.json")));
    dataContext.stockpileRegistry = std::make_unique<ac::StockpileRegistry>();
    dataContext.stockpileRegistry->Load(FixturePath("stockpiles.json"));
    dataContext.popTypeRegistry = std::make_unique<ac::PopTypeRegistry>();
    dataContext.popTypeRegistry->Load(FixturePath("pop_types.json"));
    dataContext.growthConfig = std::make_unique<ac::GrowthConfig_t>(FixtureGrowthConfig());
    dataContext.productionConfig = std::make_unique<ac::ProductionConfig_t>(
        ac::ProductionConfigParser{}.ParseConfig(FixturePath("production.json")));
    dataContext.commerceConfig = std::make_unique<ac::CommerceConfig_t>(
        ac::CommerceConfigParser{}.ParseConfig(FixturePath("commerce.json")));
    dataContext.commerceCalculator = std::make_unique<ac::CommerceCalculator>(
        *dataContext.commerceConfig, *dataContext.luaRuntime);
    dataContext.diplomacyConfig = std::make_unique<ac::DiplomacyConfig_t>(
        ac::DiplomacyConfigParser{}.ParseConfig(FixturePath("diplomacy.json")));
    dataContext.combatConfig = std::make_unique<ac::CombatConfig_t>(
        ac::CombatConfigParser{}.ParseConfig(FixturePath("combat.json")));
    dataContext.diplomacyCalculator =
        std::make_unique<ac::DiplomacyCalculator>(*dataContext.diplomacyConfig);
    dataContext.hurryProductionCalculator =
        std::make_unique<ac::HurryProductionCalculator>(*dataContext.productionConfig,
                                                       *dataContext.luaRuntime);
    dataContext.scrapRefundCalculator =
        std::make_unique<ac::ScrapRefundCalculator>(*dataContext.productionConfig,
                                                    *dataContext.luaRuntime);
    dataContext.techRegistry = std::make_unique<ac::TechRegistry>();
    dataContext.techRegistry->Load(FixturePath("techs.json"));
    // Trivial formula; tests that care about tech cost supply their own.
    dataContext.techCostConfig =
        std::make_unique<ac::TechCostConfig_t>(
            ac::TechCostConfig_t{k_TestTechCostFormula});
    dataContext.techCostCalculator = std::make_unique<ac::TechCostCalculator>(
        *dataContext.techCostConfig, *dataContext.luaRuntime);
    dataContext.socialPolicyRegistry = std::make_unique<ac::SocialPolicyRegistry>();
    dataContext.socialPolicyRegistry->Load(FixturePath("social_policies.json"));
    dataContext.socialRatingRegistry = std::make_unique<ac::SocialRatingRegistry>();
    dataContext.socialRatingRegistry->Load(FixturePath("social_rating_effects.json"));
    dataContext.policeRules = ac::PoliceRulesConfigParser{}.ParseConfig(
        FixturePath("police_rules.json"));
    dataContext.popTypeAvailabilityCalculator =
        std::make_unique<ac::PopTypeAvailabilityCalculator>(*dataContext.popTypeRegistry);
    dataContext.popCompositionConfig = std::make_unique<ac::PopCompositionConfig_t>(
        ac::PopCompositionConfigParser{}.ParseConfig(FixturePath("pop_composition.json")));
    dataContext.droneCalculator = std::make_unique<ac::DroneCalculator>(
        *dataContext.popCompositionConfig, *dataContext.luaRuntime);
    dataContext.popCompositionCalculator = std::make_unique<ac::PopCompositionCalculator>(
        *dataContext.popCompositionConfig, *dataContext.popTypeRegistry);
    dataContext.difficultyConfig = std::make_unique<ac::DifficultyConfig_t>(
        ac::DifficultyConfigParser{}.ParseConfig(FixturePath("difficulty.json")));
    dataContext.atrocitiesConfig = std::make_unique<ac::AtrocitiesConfig_t>(
        ac::AtrocityConfigParser{}.ParseConfig(FixturePath("atrocities.json")));
    dataContext.nativeLifeLevelConfig = std::make_unique<ac::NativeLifeLevelConfig_t>(
        ac::NativeLifeLevelConfigParser{}.ParseConfig(FixturePath("native_life_levels.json")));
    dataContext.worldEventsConfig = std::make_unique<ac::WorldEventsConfig_t>(
        ac::WorldEventsConfigParser{}.ParseConfig(FixturePath("world_events.json")));
    dataContext.probeActionsConfig = std::make_unique<ac::ProbeActionsConfig_t>(
        ac::ProbeActionConfigParser{}.ParseConfig(FixturePath("probe_actions.json")));
    dataContext.probeCalculator =
        std::make_unique<ac::ProbeCalculator>(*dataContext.luaRuntime);
    // GameState builds its council from these, enacting the initially active proposals.
    dataContext.councilProposalRegistry = std::make_unique<ac::CouncilProposalRegistry>();
    dataContext.councilProposalRegistry->Load(FixturePath("council/proposals.json"));
    dataContext.councilRules = std::make_unique<ac::CouncilRulesConfig_t>(
        ac::CouncilRulesConfigParser{}.ParseConfig(FixturePath("council/rules.json")));
    // Built before any Faction: the pool holds a reference into `effects`. Tests that want
    // a different formula edit damageFormula in place.
    dataContext.ecoDamageConfig = std::make_unique<ac::EcoDamageConfig_t>(
        ac::EcoDamageConfigParser{}.ParseConfig(FixturePath("eco_damage.json"),
                                                FixturePath("eco_damage.lua"),
                                                *dataContext.luaRuntime));
    dataContext.ecoDamageCalculator = std::make_unique<ac::EcoDamageCalculator>(
        *dataContext.ecoDamageConfig, *dataContext.luaRuntime);
    // Built here, before any Faction exists: FactionEffectsPool holds a reference into
    // `effects`, so a test replacing this config later would dangle it.
    dataContext.baseConquestConfig = std::make_unique<ac::BaseConquestConfig_t>();
    SetBaseConquestStat(*dataContext.baseConquestConfig,
                        ac::StatId_t::LastDefenderPopLoss, 1);
    SetBaseConquestStat(*dataContext.baseConquestConfig, ac::StatId_t::CapturePopLoss, 1);
    SetBaseConquestStat(*dataContext.baseConquestConfig,
                        ac::StatId_t::CaptureFacilitiesDestroyedMin, 0);
    SetBaseConquestStat(*dataContext.baseConquestConfig,
                        ac::StatId_t::CaptureFacilitiesDestroyedMaxPercent, 50);
    SetBaseConquestStat(*dataContext.baseConquestConfig, ac::StatId_t::ConqueredDroneCap, -0.5);
    dataContext.baseConquestConfig->nativeRaid.facilityChancePercent = 50;
    dataContext.baseConquestConfig->nativeRaid.popLoss = 1;
}

ac::MoraleCalculator& SessionInputs::morale() const
{
    return *dataContext.moraleCalculator;
}

WorldFixture::WorldFixture(int width, int height, const OccupantFiles_t& rOccupants,
                           std::vector<ac::EffectConfig_t> worldRules,
                           const ac::MapRulesConfig_t& rMapRules)
    : SessionInputs(rOccupants, std::move(worldRules))
    , state(std::make_unique<ac::WorldMap>(width, height, rMapRules, ac::Rainfall_t::Average),
            dataContext, settings, k_TestRngSeed)
    , scope(state)
    , map(state.GetWorldMap())
    , ctx(state.GetTileEffects())
{
}

ac::Tile& WorldFixture::At(int x, int y)
{
    return *map.GetTile(x, y);
}

BaseFixture::BaseFixture()
    : BaseFixture(k_TestMapWidth, k_TestMapHeight)
{
}

BaseFixture::BaseFixture(int width, int height, const OccupantFiles_t& rOccupants,
                         std::vector<ac::EffectConfig_t> worldRules,
                         const ac::MapRulesConfig_t& rMapRules)
    : WorldFixture(width, height, rOccupants, std::move(worldRules), rMapRules)
{
    ownerDefinition.id = "test_base_owner";
}

ac::BuildingRegistry& BaseFixture::buildings()
{
    return *dataContext.buildingRegistry;
}

const ac::BuildingRegistry& BaseFixture::buildings() const
{
    return *dataContext.buildingRegistry;
}

ac::StockpileRegistry& BaseFixture::stockpiles()
{
    return *dataContext.stockpileRegistry;
}

const ac::StockpileRegistry& BaseFixture::stockpiles() const
{
    return *dataContext.stockpileRegistry;
}

ac::PopTypeRegistry& BaseFixture::popTypes()
{
    return *dataContext.popTypeRegistry;
}

const ac::PopTypeRegistry& BaseFixture::popTypes() const
{
    return *dataContext.popTypeRegistry;
}

ac::Faction& BaseFixture::MakeFaction()
{
    return MakeFaction_(factionDefinition);
}

ac::Faction& BaseFixture::MakeFaction(const ac::FactionConfig_t& rDefinition)
{
    extraDefinitions.push_back(std::make_unique<ac::FactionConfig_t>(rDefinition));
    return MakeFaction_(*extraDefinitions.back());
}

ac::Faction& BaseFixture::MakeFaction_(const ac::FactionConfig_t& rDefinition)
{
    // Only the first session faction is player-controlled; tests needing a second
    // faction (e.g. WorldGlobal routing between two factions) get an AI one.
    const bool bIsPlayerControlled = state.Factions().empty();
    const ac::FactionId_t id = state.AllocateFactionId();
    // Distinct per faction so two fixture factions do not make identical picks.
    const uint32_t seed = k_TestFactionSeed + static_cast<uint32_t>(id);
    return state.AddFaction(std::make_unique<ac::Faction>(
        id, bIsPlayerControlled, rDefinition, dataContext, map, settings, state.GetUnitIdAllocator(), seed));
}

ac::Faction& BaseFixture::BaseOwner()
{
    if (pBaseOwner == nullptr)
    {
        pBaseOwner = &MakeFaction_(ownerDefinition);
    }
    return *pBaseOwner;
}

ac::BaseManager& BaseFixture::MakeFactionBase(ac::Faction& rFaction, int x, int y,
                                              std::optional<int> initialPopulation)
{
    auto pBase = std::make_unique<ac::BaseManager>(
        rFaction, state.AllocateBaseId(), "TestBase", At(x, y),
        ac::MakeBaseRuleset(dataContext),
        ctx,
        initialPopulation);
    ac::BaseManager& rBase = *pBase;
    rFaction.AddBase(std::move(pBase));
    return rBase;
}

ac::BaseManager& BaseFixture::MakeBase(int x, int y, std::optional<int> initialPopulation)
{
    return MakeFactionBase(BaseOwner(), x, y, initialPopulation);
}

FactionFixture::FactionFixture()
    : FactionFixture(k_TestMapWidth, k_TestMapHeight)
{
}

FactionFixture::FactionFixture(int width, int height, std::vector<ac::EffectConfig_t> worldRules,
                               const OccupantFiles_t& rOccupants,
                               const ac::MapRulesConfig_t& rMapRules)
    : BaseFixture(width, height, rOccupants, std::move(worldRules), rMapRules)
{
    factionDefinition.id = "test_faction";
}

ac::SocialPolicyRegistry& FactionFixture::socialPolicies()
{
    return *dataContext.socialPolicyRegistry;
}

const ac::SocialPolicyRegistry& FactionFixture::socialPolicies() const
{
    return *dataContext.socialPolicyRegistry;
}

ac::SocialRatingRegistry& FactionFixture::socialRatings()
{
    return *dataContext.socialRatingRegistry;
}

const ac::SocialRatingRegistry& FactionFixture::socialRatings() const
{
    return *dataContext.socialRatingRegistry;
}

const ac::UnitDesign& FactionFixture::MakeDesign(const std::vector<std::string>& rComponentIds)
{
    std::vector<ac::UnitSlotConfig_t> slots;
    std::unordered_map<std::string, const ac::UnitComponentConfig_t*> assigned;
    int slotIndex = 0;
    for (const std::string& rId : rComponentIds)
    {
        const ac::UnitComponentConfig_t* pComponent = unitComponents.Find(rId);
        if (!pComponent)
        {
            throw std::runtime_error("Fixture component not found: " + rId);
        }
        ac::UnitSlotConfig_t slot;
        slot.id = "slot_" + std::to_string(slotIndex++);
        slot.displayName = slot.id;
        slot.componentType = pComponent->type;
        slot.required = true;
        assigned[slot.id] = pComponent;
        slots.push_back(slot);
    }
    return designs.emplace_back(slots, assigned);
}

ac::Unit& FactionFixture::MakeUnit(ac::Faction& rFaction, int x, int y,
                                   const std::vector<std::string>& rComponentIds,
                                   ac::BaseManager* pHomeBase)
{
    return rFaction.GetUnitManager().CreateUnit(state.AllocateUnitId(), MakeDesign(rComponentIds),
                                                map.GetUnitPositions(), At(x, y),
                                                ac::UnitOrigin::Gift(pHomeBase));
}

ac::Unit& FactionFixture::MakeBuiltUnit(ac::Faction& rFaction, int x, int y,
                                        const std::vector<std::string>& rComponentIds,
                                        ac::BaseManager& rBuiltAt)
{
    return rFaction.GetUnitManager().CreateUnit(state.AllocateUnitId(), MakeDesign(rComponentIds),
                                                map.GetUnitPositions(), At(x, y),
                                                ac::UnitOrigin::Built(rBuiltAt));
}

void FactionFixture::MoveUnit(ac::Unit& rUnit, int x, int y)
{
    // No pre-check here: UnitPositionIndex::MoveUnit enforces the stacking rule itself now.
    // The guard this used to duplicate threw a different exception type than production,
    // so a fixture move and a real move failed differently.
    map.GetUnitPositions().MoveUnit(rUnit, At(x, y));
}

void InstallNativeUnits(ac::GameDataContext& rData, int lifeformsMin, int lifeformsMax)
{
    std::ifstream fixture(FixturePath("native_units.json"));
    nlohmann::json config = nlohmann::json::parse(fixture);
    config["fungal_bloom_native_lifeforms_min"] = lifeformsMin;
    config["fungal_bloom_native_lifeforms_max"] = lifeformsMax;
    const TempConfigFile file("native_units.json", config.dump());
    rData.nativeUnitRegistry = std::make_unique<ac::NativeUnitRegistry>();
    rData.nativeUnitRegistry->Load(file.Path());
}

void AdvanceToYear(ac::GameState& rState, int year)
{
    if (year < rState.GetMissionYear())
    {
        throw std::logic_error("AdvanceToYear: year " + std::to_string(year) + " is already past");
    }
    while (rState.GetMissionYear() < year)
    {
        rState.IncrementMissionYear();
    }
}

void FillLand(ac::WorldMap& rMap)
{
    for (auto& pTile : rMap.GetTiles())
    {
        pTile->SetElevation(100);
    }
}

ac::Faction& AddSessionFaction(FactionFixture& rFixtures, const ac::FactionConfig_t& rDefinition,
                               bool bIsPlayerControlled)
{
    return rFixtures.state.AddFaction(std::make_unique<ac::Faction>(
        rFixtures.state.AllocateFactionId(), bIsPlayerControlled, rDefinition,
        rFixtures.dataContext, rFixtures.map, rFixtures.settings, rFixtures.state.GetUnitIdAllocator(), k_TestFactionSeed));
}

ac::Faction& AddNativeLifeFaction(FactionFixture& rFixtures)
{
    rFixtures.extraDefinitions.push_back(
        std::make_unique<ac::FactionConfig_t>(rFixtures.factionDefinition));
    ac::FactionConfig_t& rPlanet = *rFixtures.extraDefinitions.back();
    rPlanet.id = "planet";
    rPlanet.identity.species = ac::FactionSpecies_t::NativeLife;
    rPlanet.identity.participatesInCouncil = false;
    return AddSessionFaction(rFixtures, rPlanet, /*bIsPlayerControlled=*/false);
}

ac::BaseManager& MakeSessionBase(FactionFixture& rFixtures, ac::Faction& rFaction, int x, int y)
{
    ac::BaseManager* pBase = rFaction.CreateBase(
        rFixtures.state.AllocateBaseId(), "TestBase", rFixtures.map.GetTile(x, y),
        rFixtures.ctx, std::nullopt);
    if (!pBase)
    {
        throw std::runtime_error("MakeSessionBase: CreateBase failed");
    }
    return *pBase;
}

ac::CombatResult_t ResolveWithCollateral(ac::CombatResolver& rCombat, ac::WorldMap& rMap,
                                         ac::Unit& rAttacker, ac::Unit& rDefender)
{
    const ac::Tile& rTile = rDefender.GetTile();
    ac::CombatResult_t result = rCombat.Resolve(rAttacker, rDefender);
    if (result.bDefenderDestroyed)
    {
        const std::vector<ac::UnitId_t> captured =
            ac::ApplyStackCollateral(rMap.GetUnitPositions(), rAttacker, rTile);
        result.capturedUnitIds.insert(result.capturedUnitIds.end(), captured.begin(),
                                      captured.end());
    }
    return result;
}

std::optional<ac::CombatResult_t> AttackTile(ac::UnitAttackExecutor& rAttacks, ac::Unit& rAttacker,
                                             const ac::Tile& rTargetTile)
{
    ac::Unit* pDefender = ac::FindAttackableHostileOnTile(rAttacker, rTargetTile);
    if (!pDefender)
    {
        return std::nullopt;
    }
    return rAttacks.TryAttack(rAttacker, *pDefender);
}

} // namespace actest
