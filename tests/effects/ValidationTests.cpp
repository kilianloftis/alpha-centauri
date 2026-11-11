// Tests for post-load effect reference validation (EffectReferenceValidator) and the
// compile-time single sources of truth: scope routing (LaneFor/IsFactionLane) and stat
// seed semantics (KindFor/SeedFor).

#include "TempConfigFile.h"
#include "TestTempDir.h"
#include "game/natives/NativeLifeConfig.h"
#include "game/ecology/EcoDamageConfig.h"
#include "game/world-events/WorldEventConfig.h"
#include "game/supply/SupplyPodConfig.h"
#include "game/EffectReferenceValidator.h"
#include "game/GameDataContext.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/production/StockpileRegistry.h"
#include "game/atrocities/AtrocityConfig.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRulesConfig.h"
#include "game/faction/FactionRegistry.h"
#include "game/production/ProductionConfigParser.h"
#include "game/economy/CommerceConfig.h"
#include "game/diplomacy/DiplomacyConfig.h"
#include "game/combat/CombatConfig.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/TerrainConfig.h"
#include "game/map/TerrainFeatureValidation.h"
#include "game/map/MapOccupantLoad.h"
#include "game/map/TerrainOperationRegistry.h"
#include "game/population/PopTypeRegistry.h"
#include "game/research/TechRegistry.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/probes/ProbeActionConfig.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/natives/NativeUnitRegistry.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"
#include "game/DifficultyConfig.h"
#include "game/base/BaseConquestConfig.h"
#include "game/population/PopCompositionConfigParser.h"

#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>

using namespace ac;

namespace
{

EffectConfig_t GrantBuilding(std::string buildingId)
{
    EffectConfig_t config;
    config.effect = GrantBuildingEffect_t{std::move(buildingId)};
    config.scope = EffectScope_t::ThisBase;
    return config;
}

// Empty instances of every target / effect-source LoadGameData installs before validation.
void FillEffectReferenceContext(GameDataContext& rData)
{
    rData.buildingRegistry = std::make_unique<BuildingRegistry>();
    rData.stockpileRegistry = std::make_unique<StockpileRegistry>();
    rData.improvementRegistry = std::make_unique<ImprovementRegistry>();
    rData.techRegistry = std::make_unique<TechRegistry>();
    rData.unitComponentRegistry = std::make_unique<UnitComponentRegistry>();
    rData.nativeUnitRegistry = std::make_unique<NativeUnitRegistry>();
    rData.popTypeRegistry = std::make_unique<PopTypeRegistry>();
    rData.socialPolicyRegistry = std::make_unique<SocialPolicyRegistry>();
    rData.socialRatingRegistry = std::make_unique<SocialRatingRegistry>();
    rData.factionRegistry = std::make_unique<FactionRegistry>();
    rData.councilProposalRegistry = std::make_unique<CouncilProposalRegistry>();
    rData.councilRules = std::make_unique<CouncilRulesConfig_t>();
    rData.probeActionsConfig = std::make_unique<ProbeActionsConfig_t>();
    rData.productionConfig = std::make_unique<ProductionConfig_t>();
    rData.commerceConfig = std::make_unique<CommerceConfig_t>();
    rData.diplomacyConfig = std::make_unique<DiplomacyConfig_t>();
    rData.combatConfig = std::make_unique<CombatConfig_t>();
    rData.difficultyConfig = std::make_unique<DifficultyConfig_t>();
    rData.atrocitiesConfig = std::make_unique<AtrocitiesConfig_t>();
    rData.nativeLifeConfig = std::make_unique<NativeLifeConfig_t>();
    rData.worldEventsConfig = std::make_unique<WorldEventsConfig_t>();
    rData.supplyPodConfig = std::make_unique<SupplyPodConfig_t>();
    rData.ecoDamageConfig = std::make_unique<EcoDamageConfig_t>();
    rData.baseConquestConfig = std::make_unique<BaseConquestConfig_t>();
    rData.popCompositionConfig = std::make_unique<PopCompositionConfig_t>();
    rData.terrainOperationRegistry = std::make_unique<TerrainOperationRegistry>();
}

// The buildings the reference tests name, with no effects of their own to validate.
constexpr const char* k_ReferenceBuildings = R"([
    { "id": "granted_hall", "name": "Granted Hall", "mineral_cost": 10, "category": "grow" },
    { "id": "upkeep_hall", "name": "Upkeep Hall", "mineral_cost": 10, "category": "build" },
    { "id": "Network_Node", "name": "Network Node", "mineral_cost": 10, "category": "discover" },
    { "id": "Pressure_Dome", "name": "Pressure Dome", "mineral_cost": 10, "category": "build" }
])";

// The context above with the registries the reference tests check ids against. Every effect
// source stays empty, so a test fills the one it validates.
void LoadFixtureRegistries(GameDataContext& rData)
{
    FillEffectReferenceContext(rData);
    const actest::TempConfigFile buildings("reference_buildings.json", k_ReferenceBuildings);
    rData.buildingRegistry->Load(buildings.Path());
    rData.unitComponentRegistry->Load(actest::FixturePath("unit_components.json"));
    rData.nativeUnitRegistry->Load(actest::FixturePath("native_units_full.json"));
    rData.socialRatingRegistry->Load(actest::FixturePath("social_rating_effects.json"));
    LoadMapOccupants(actest::FixturePath("improvements.json"), actest::FixturePath("terrain.json"),
                     *rData.improvementRegistry, *rData.terrainOperationRegistry);
}

} // namespace

// LaneFor is the compile-time single source of truth for scope routing; pin every scope's
// lane so a new scope (which the compiler forces into LaneFor's switch) gets a deliberate
// routing decision here too.
static_assert(LaneFor(EffectScope_t::ThisBase) == EffectLane_t::Base);
static_assert(LaneFor(EffectScope_t::AllOwnerBases) == EffectLane_t::FactionWide);
static_assert(LaneFor(EffectScope_t::FactionGlobal) == EffectLane_t::FactionWide);
static_assert(LaneFor(EffectScope_t::WorldGlobal) == EffectLane_t::FactionWide);
static_assert(LaneFor(EffectScope_t::FactionUnits) == EffectLane_t::FactionUnits);
static_assert(LaneFor(EffectScope_t::ProducedAtThisBase) == EffectLane_t::ProducedAtBase);
static_assert(LaneFor(EffectScope_t::ThisUnit) == EffectLane_t::UnitLocal);
static_assert(LaneFor(EffectScope_t::ThisPop) == EffectLane_t::PopLocal);
static_assert(LaneFor(EffectScope_t::ThisTile) == EffectLane_t::TileLocal);
static_assert(LaneFor(EffectScope_t::ThisTech) == EffectLane_t::TechLocal);
static_assert(LaneFor(EffectScope_t::FactionPair) == EffectLane_t::FactionPair);

static_assert(!IsFactionLane(EffectScope_t::ThisBase));
static_assert(IsFactionLane(EffectScope_t::AllOwnerBases));
static_assert(IsFactionLane(EffectScope_t::FactionGlobal));
static_assert(IsFactionLane(EffectScope_t::WorldGlobal));
static_assert(IsFactionLane(EffectScope_t::FactionUnits));
static_assert(!IsFactionLane(EffectScope_t::ProducedAtThisBase));
static_assert(!IsFactionLane(EffectScope_t::ThisUnit));
static_assert(!IsFactionLane(EffectScope_t::ThisPop));
static_assert(!IsFactionLane(EffectScope_t::ThisTile));
static_assert(!IsFactionLane(EffectScope_t::ThisTech));

static_assert(TagsOriginBase(EffectScope_t::ThisBase));
static_assert(TagsOriginBase(EffectScope_t::ProducedAtThisBase));
static_assert(TagsOriginBase(EffectScope_t::FactionUnits));
static_assert(!TagsOriginBase(EffectScope_t::AllOwnerBases));
static_assert(!TagsOriginBase(EffectScope_t::FactionGlobal));
static_assert(!TagsOriginBase(EffectScope_t::WorldGlobal));
static_assert(!TagsOriginBase(EffectScope_t::ThisUnit));
static_assert(!TagsOriginBase(EffectScope_t::ThisPop));
static_assert(!TagsOriginBase(EffectScope_t::ThisTile));
static_assert(!TagsOriginBase(EffectScope_t::ThisTech));

// KindFor is the same single-source-of-truth pattern for stat seed semantics: pin every
// stat's kind so a new StatId_t (which the compiler forces into KindFor's switch) gets a
// deliberate seed decision here too.
static_assert(KindFor(StatId_t::Nutrients) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Minerals) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Energy) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::EnergyCredits) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Econ) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Labs) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Psych) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Drones) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Talents) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Attack) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Defense) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::TileDefense) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::Movement) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Vision) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::HitPoints) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::PsiDamage) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::CollateralDamage) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::EarthquakeLevels) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::FungalBloomTiles) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::ExplosionRadius) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::CollateralSusceptibility) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::PlanetPearls) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::DisengageChance) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::BoardChance) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::BoardHpLimit) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::TurnsOfFuel) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::DamageFromOutOfFuel) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::AirdropRange) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::AirdropLandingDamage) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::BombardRange) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::BombardMinHpPercent) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::CargoCapacity) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::DifficultTerrainCost) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::MoveCost) == StatKind_t::RawScaled);
static_assert(KindFor(StatId_t::MineralUpkeep) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::FreeUnitSupport) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::MaxPolice) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::PoliceEffectiveness) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::AwayFromHomeDrones) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::SizeFreeDrones) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::StartingMinerals) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::MoraleRank) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::CostMultiplier) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::PrototypeSurchargeScale) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::RetoolPenaltyScale) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::UpgradeCostScale) == StatKind_t::PureMultiplier);
static_assert(DomainFor(StatId_t::UpgradeCostScale) == ResolveDomain_t::Faction);
static_assert(KindFor(StatId_t::FacilityEnergyUpkeep) == StatKind_t::RawScaled);
static_assert(KindFor(StatId_t::ProbeActionCost) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::ProbeActorCost) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::ProbeDistanceScale) == StatKind_t::PureMultiplier);
static_assert(DomainFor(StatId_t::ProbeDistanceScale) == ResolveDomain_t::Base);
static_assert(DomainFor(StatId_t::ProbeActorCost) == ResolveDomain_t::Faction);
static_assert(KindFor(StatId_t::ProbeDefense) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::ProbeFailureScale) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::ProbeSuccessScale) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::PlagueDamageScale) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::ProbeHunter) == StatKind_t::Additive);
static_assert(DomainFor(StatId_t::ProbeHunter) == ResolveDomain_t::Base);
static_assert(KindFor(StatId_t::ProbeSuccessBust) == StatKind_t::Additive);
static_assert(DomainFor(StatId_t::ProbeSuccessBust) == ResolveDomain_t::Base);
static_assert(KindFor(StatId_t::ProbeIntrusion) == StatKind_t::Additive);
static_assert(DomainFor(StatId_t::ProbeIntrusion) == ResolveDomain_t::Unit);
static_assert(KindFor(StatId_t::ProbeFailureSave) == StatKind_t::Additive);
static_assert(DomainFor(StatId_t::ProbeFailureSave) == ResolveDomain_t::Unit);
static_assert(KindFor(StatId_t::GarrisonProbeWeight) == StatKind_t::Additive);
static_assert(DomainFor(StatId_t::GarrisonProbeWeight) == ResolveDomain_t::Unit);
static_assert(KindFor(StatId_t::GarrisonProbeMultiplier) == StatKind_t::Additive);
static_assert(DomainFor(StatId_t::GarrisonProbeMultiplier) == ResolveDomain_t::Unit);
static_assert(DomainFor(StatId_t::PlagueDamageScale) == ResolveDomain_t::Base);
static_assert(KindFor(StatId_t::MoraleExtra) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::PromotionEligibilityChance) == StatKind_t::RawScaled);
static_assert(KindFor(StatId_t::PromotionSuccessChance) == StatKind_t::RawScaled);
static_assert(KindFor(StatId_t::GrowthRate) == StatKind_t::RawScaled);
static_assert(KindFor(StatId_t::StartingSize) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::MaxBaseSize) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::TechCost) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::TechCostDiff) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::TechCostCatchUp) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::TechCostTurnLag) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::TechCostResearch) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::UpheavalCost) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::UpheavalCostScale) == StatKind_t::RawScaled);
static_assert(KindFor(StatId_t::Bureaucracy) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::MoistureTier) == StatKind_t::RawScaled);
static_assert(KindFor(StatId_t::CommerceRate) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::CommerceRating) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::CouncilVotes) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::CommerceEnergyBonus) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::InefficiencyDenominator) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::AllocationImbalanceFactor) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::ScrapRefund) == StatKind_t::RawScaled);
// Additive, not RawScaled: base_conquest.json's effects list supplies the whole baseline.
static_assert(KindFor(StatId_t::LastDefenderPopLoss) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::CaptureFacilitiesDestroyed) == StatKind_t::RawScaled);
static_assert(KindFor(StatId_t::CapturePopLoss) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::ConqueredDroneCap) == StatKind_t::Additive);
static_assert(DomainFor(StatId_t::CaptureFacilitiesDestroyed) == ResolveDomain_t::Base);
// A pure product of difficulty x Planet rating x native life x perihelion; the eco formula
// multiplies by it once, so there is no raw value for it to scale.
static_assert(KindFor(StatId_t::EcologicalDamage) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::EcoDamageContribution) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::EcoDamageWorkedContribution) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::EcoTerraformScale) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::EcoCleanMinerals) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::EcoDamageReduction) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::EcoMineralOffset) == StatKind_t::Additive);
static_assert(DomainFor(StatId_t::EcoDamageContribution) == ResolveDomain_t::Tile);
static_assert(DomainFor(StatId_t::EcoDamageWorkedContribution) == ResolveDomain_t::Tile);
static_assert(DomainFor(StatId_t::EcoTerraformScale) == ResolveDomain_t::Base);
static_assert(DomainFor(StatId_t::EcoCleanMinerals) == ResolveDomain_t::Faction);
static_assert(DomainFor(StatId_t::EcoDamageReduction) == ResolveDomain_t::Base);
static_assert(DomainFor(StatId_t::EcoMineralOffset) == ResolveDomain_t::Base);
static_assert(KindFor(StatId_t::RebelJoinWeight) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::GiftGivenValue) == StatKind_t::RawScaled);
static_assert(KindFor(StatId_t::GiftReceivedValue) == StatKind_t::RawScaled);
static_assert(KindFor(StatId_t::GiftSizeThreshold) == StatKind_t::PureMultiplier);

// SeedFor derives the context-free seed from the kind; RawScaled stats have none (SeedFor
// throws for them, which is not constexpr-evaluable, so no pin here).
static_assert(SeedFor(StatId_t::Nutrients) == 0.0);
static_assert(SeedFor(StatId_t::Attack) == 0.0);
static_assert(SeedFor(StatId_t::CostMultiplier) == 1.0);
static_assert(SeedFor(StatId_t::PrototypeSurchargeScale) == 1.0);
static_assert(SeedFor(StatId_t::RetoolPenaltyScale) == 1.0);
static_assert(SeedFor(StatId_t::UpgradeCostScale) == 1.0);
static_assert(SeedFor(StatId_t::TileDefense) == 1.0);
static_assert(SeedFor(StatId_t::RebelJoinWeight) == 0.0);
static_assert(SeedFor(StatId_t::GiftSizeThreshold) == 1.0);

// DomainFor is the consumer-subject counterpart of KindFor: pin every stat's resolve domain.
static_assert(DomainFor(StatId_t::Nutrients) == ResolveDomain_t::Base);
static_assert(DomainFor(StatId_t::Drones) == ResolveDomain_t::Base);
static_assert(DomainFor(StatId_t::CostMultiplier) == ResolveDomain_t::Base);
static_assert(DomainFor(StatId_t::StartingMinerals) == ResolveDomain_t::Base);
static_assert(DomainFor(StatId_t::StartingSize) == ResolveDomain_t::Base);
static_assert(DomainFor(StatId_t::MaxBaseSize) == ResolveDomain_t::Base);
static_assert(DomainFor(StatId_t::ProbeDefense) == ResolveDomain_t::Base);
static_assert(DomainFor(StatId_t::TechCost) == ResolveDomain_t::Faction);
static_assert(DomainFor(StatId_t::TechCostCatchUp) == ResolveDomain_t::Faction);
static_assert(DomainFor(StatId_t::TechCostTurnLag) == ResolveDomain_t::Faction);
static_assert(DomainFor(StatId_t::TechCostResearch) == ResolveDomain_t::Faction);
static_assert(DomainFor(StatId_t::UpheavalCost) == ResolveDomain_t::Faction);
static_assert(DomainFor(StatId_t::UpheavalCostScale) == ResolveDomain_t::Faction);
static_assert(DomainFor(StatId_t::CouncilVotes) == ResolveDomain_t::Faction);
static_assert(DomainFor(StatId_t::CommerceRate) == ResolveDomain_t::Faction);
static_assert(DomainFor(StatId_t::CommerceRating) == ResolveDomain_t::Base);
static_assert(DomainFor(StatId_t::RebelJoinWeight) == ResolveDomain_t::Faction);
static_assert(DomainFor(StatId_t::GiftGivenValue) == ResolveDomain_t::Faction);
static_assert(DomainFor(StatId_t::GiftReceivedValue) == ResolveDomain_t::Faction);
static_assert(DomainFor(StatId_t::GiftSizeThreshold) == ResolveDomain_t::Faction);
static_assert(DomainFor(StatId_t::Attack) == ResolveDomain_t::Unit);
static_assert(DomainFor(StatId_t::EarthquakeLevels) == ResolveDomain_t::Unit);
static_assert(DomainFor(StatId_t::FungalBloomTiles) == ResolveDomain_t::Unit);
static_assert(DomainFor(StatId_t::ExplosionRadius) == ResolveDomain_t::Unit);
static_assert(DomainFor(StatId_t::CollateralSusceptibility) == ResolveDomain_t::Unit);
static_assert(DomainFor(StatId_t::Defense) == ResolveDomain_t::Unit);
static_assert(DomainFor(StatId_t::MoraleRank) == ResolveDomain_t::Unit);
static_assert(DomainFor(StatId_t::MoraleExtra) == ResolveDomain_t::Unit);
static_assert(DomainFor(StatId_t::MoistureTier) == ResolveDomain_t::Tile);
static_assert(DomainFor(StatId_t::TileDefense) == ResolveDomain_t::Tile);
static_assert(DomainFor(StatId_t::MoveCost) == ResolveDomain_t::Tile);
static_assert(DomainFor(StatId_t::BombardRange) == ResolveDomain_t::Unit);
static_assert(DomainFor(StatId_t::BombardMinHpPercent) == ResolveDomain_t::Tile);
static_assert(DomainFor(StatId_t::BoardChance) == ResolveDomain_t::Unit);
static_assert(DomainFor(StatId_t::BoardHpLimit) == ResolveDomain_t::Unit);

TEST_CASE("ValidateEffectReferences: GrantBuilding targets must exist", "[effects][validation]")
{
    GameDataContext data;
    LoadFixtureRegistries(data);

    data.productionConfig->effects = {GrantBuilding("granted_hall")};
    CHECK_NOTHROW(ValidateEffectReferences(data));

    data.productionConfig->effects = {GrantBuilding("no_such_building")};
    CHECK_THROWS_WITH(ValidateEffectReferences(data),
                      Catch::Matchers::ContainsSubstring("no_such_building"));
}

TEST_CASE("ValidateEffectReferences: social rating axes must have a table",
          "[effects][validation]")
{
    // SocialRatingResolver looks up the axis table whenever the accumulated total is non-zero,
    // which is the first turn after a player adopts a policy declaring the modifier. A modifier
    // naming an axis with no table has to fail at load, not there.
    // The fixture unit components name the morale axis, so it stays.
    const actest::TempConfigFile noPlanet("no_planet_ratings.json", R"([
        { "id": "growth", "levels": { "2": [] } },
        { "id": "morale", "levels": { "2": [] } }
    ])");
    GameDataContext data;
    LoadFixtureRegistries(data);
    data.socialRatingRegistry->Load(noPlanet.Path());

    const auto ratingModifier = [](SocialRatingId_t rating) {
        EffectConfig_t config;
        config.effect = SocialRatingModifierEffect_t{rating, 2};
        config.scope = EffectScope_t::FactionGlobal;
        return std::vector<EffectConfig_t>{config};
    };

    data.productionConfig->effects = ratingModifier(SocialRatingId_t::Growth);
    CHECK_NOTHROW(ValidateEffectReferences(data));

    // The registry defines no table for Planet.
    REQUIRE(data.socialRatingRegistry->Find("planet") == nullptr);
    data.productionConfig->effects = ratingModifier(SocialRatingId_t::Planet);
    CHECK_THROWS_WITH(ValidateEffectReferences(data),
                      Catch::Matchers::ContainsSubstring("planet")
                          && Catch::Matchers::ContainsSubstring("production"));
}

TEST_CASE("ValidateEffectReferences: selector improvement ids must exist", "[effects][validation]")
{
    GameDataContext data;
    LoadFixtureRegistries(data);

    actest::EffectPool pool;
    data.productionConfig->effects = {
        pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                     actest::ImprovementSelector("Farm"))};
    CHECK_NOTHROW(ValidateEffectReferences(data));

    data.productionConfig->effects = {
        pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                     actest::ImprovementSelector("Fram"))};
    CHECK_THROWS_WITH(ValidateEffectReferences(data), Catch::Matchers::ContainsSubstring("Fram"));
}

TEST_CASE("ValidateEffectReferences: condition features accept terrain ids and improvement ids",
          "[effects][validation]")
{
    GameDataContext data;
    LoadFixtureRegistries(data);

    actest::EffectPool pool;
    // "Rocky" / "Water" are terrain ids and "Base" is an improvement id.
    data.productionConfig->effects = {
        pool.StatMod(StatId_t::Attack, 25.0, ModifierOp_t::AddPercent, EffectScope_t::ThisUnit,
                     std::nullopt, actest::TargetTileHas("Rocky")),
        pool.StatMod(StatId_t::Attack, 25.0, ModifierOp_t::AddPercent, EffectScope_t::ThisUnit,
                     std::nullopt, actest::TargetTileHas("Base")),
        pool.StatMod(StatId_t::Attack, 25.0, ModifierOp_t::AddPercent, EffectScope_t::ThisUnit,
                     std::nullopt, actest::TargetTileHas("Water"))};
    CHECK_NOTHROW(ValidateEffectReferences(data));

    data.productionConfig->effects = {
        pool.StatMod(StatId_t::Attack, 25.0, ModifierOp_t::AddPercent, EffectScope_t::ThisUnit,
                     std::nullopt, actest::TargetTileHas("Swamp"))};
    CHECK_THROWS_WITH(ValidateEffectReferences(data), Catch::Matchers::ContainsSubstring("Swamp"));
}

TEST_CASE("ValidateTerrainFeatures: every intrinsic terrain id must have a terrain entry",
          "[effects][validation][terrain]")
{
    ImprovementRegistry occupants;
    TerrainOperationRegistry operations;
    LoadMapOccupants(actest::FixturePath("improvements.json"), actest::FixturePath("terrain.json"),
                     occupants, operations);
    CHECK_NOTHROW(ValidateTerrainFeatures(occupants));

    ImprovementRegistry incomplete;
    TerrainOperationRegistry incompleteOperations;
    LoadMapOccupants(actest::FixturePath("improvements.json"),
                     actest::FixturePath("improvements_missing_terrain.json"), incomplete,
                     incompleteOperations);
    CHECK_THROWS_WITH(ValidateTerrainFeatures(incomplete),
                      Catch::Matchers::ContainsSubstring("Aquifer"));
}

TEST_CASE("ValidateEffectReferences: BuildingId buildingFilter ids must exist",
          "[effects][validation][buildingFilter]")
{
    GameDataContext data;
    LoadFixtureRegistries(data);

    actest::EffectPool pool;
    data.productionConfig->effects = {
        pool.StatMod(StatId_t::FacilityEnergyUpkeep, -50.0, ModifierOp_t::AddPercent,
                     EffectScope_t::FactionGlobal, std::nullopt, std::nullopt,
                     BuildingFilterId_t{"upkeep_hall"})};
    CHECK_NOTHROW(ValidateEffectReferences(data));

    data.productionConfig->effects = {
        pool.StatMod(StatId_t::FacilityEnergyUpkeep, -50.0, ModifierOp_t::AddPercent,
                     EffectScope_t::FactionGlobal, std::nullopt, std::nullopt,
                     BuildingFilterId_t{"no_such_building"})};
    CHECK_THROWS_WITH(ValidateEffectReferences(data),
                      Catch::Matchers::ContainsSubstring("no_such_building"));
}

TEST_CASE("ValidateEffectReferences: HasComponent condition ids must exist",
          "[effects][validation][condition]")
{
    GameDataContext data;
    LoadFixtureRegistries(data);

    actest::EffectPool pool;
    data.productionConfig->effects = {
        pool.StatMod(StatId_t::Attack, 1.0, ModifierOp_t::Add, EffectScope_t::FactionUnits,
                     std::nullopt, actest::HasComponentCondition("test_weapon"))};
    CHECK_NOTHROW(ValidateEffectReferences(data));

    data.productionConfig->effects = {
        pool.StatMod(StatId_t::Attack, 1.0, ModifierOp_t::Add, EffectScope_t::FactionUnits,
                     std::nullopt, actest::HasComponentCondition("no_such_component"))};
    CHECK_THROWS_WITH(ValidateEffectReferences(data),
                      Catch::Matchers::ContainsSubstring("no_such_component"));
}

TEST_CASE("ValidateEffectReferences: triggered SubjectDesign ids must exist",
          "[effects][validation][condition]")
{
    GameDataContext data;
    LoadFixtureRegistries(data);

    TriggeredEffectConfig_t good;
    good.effect = GrantTechEffect_t{};
    good.condition = SubjectDesign_t{"Alien_Artifact"};
    data.ecoDamageConfig->onPopEffects = {good};
    CHECK_NOTHROW(ValidateEffectReferences(data));

    TriggeredEffectConfig_t bad = good;
    bad.condition = SubjectDesign_t{"No_Such_Native"};
    data.ecoDamageConfig->onPopEffects = {bad};
    CHECK_THROWS_WITH(ValidateEffectReferences(data),
                      Catch::Matchers::ContainsSubstring("No_Such_Native"));
}

TEST_CASE("ValidateEffectReferences: triggered BaseHasBuilding ids must exist",
          "[effects][validation][condition]")
{
    GameDataContext data;
    LoadFixtureRegistries(data);

    TriggeredEffectConfig_t good;
    good.effect = GrantTechEffect_t{};
    good.condition = BaseHasBuilding_t{"Network_Node"};
    data.ecoDamageConfig->onPopEffects = {good};
    CHECK_NOTHROW(ValidateEffectReferences(data));

    TriggeredEffectConfig_t bad = good;
    bad.condition = BaseHasBuilding_t{"No_Such_Building"};
    data.ecoDamageConfig->onPopEffects = {bad};
    CHECK_THROWS_WITH(ValidateEffectReferences(data),
                      Catch::Matchers::ContainsSubstring("No_Such_Building"));
}

TEST_CASE("ValidateEffectReferences(GameDataContext): null required registry throws",
          "[effects][validation]")
{
    GameDataContext empty;
    CHECK_THROWS_WITH(ValidateEffectReferences(empty),
                      Catch::Matchers::ContainsSubstring("buildingRegistry"));

    GameDataContext missingTech;
    FillEffectReferenceContext(missingTech);
    missingTech.techRegistry.reset();
    CHECK_THROWS_WITH(ValidateEffectReferences(missingTech),
                      Catch::Matchers::ContainsSubstring("techRegistry"));

    GameDataContext missingProduction;
    FillEffectReferenceContext(missingProduction);
    missingProduction.productionConfig.reset();
    CHECK_THROWS_WITH(ValidateEffectReferences(missingProduction),
                      Catch::Matchers::ContainsSubstring("productionConfig"));

    GameDataContext complete;
    FillEffectReferenceContext(complete);
    CHECK_NOTHROW(ValidateEffectReferences(complete));
}

TEST_CASE("ValidateEffectReferences(GameDataContext): probe action effect ids are checked",
          "[effects][validation][probe]")
{
    // Minimal building registry — the full buildings fixture grants techs that would
    // fail against an empty TechRegistry before the probe walk is reached.
    const std::filesystem::path buildingsPath =
        actest::TestTempDir::Path() / "ac_probe_effect_buildings.json";
    {
        std::ofstream out(buildingsPath);
        out << R"([
            { "id": "granted_hall", "name": "Granted Hall", "mineral_cost": 10, "category": "grow" }
        ])";
    }

    GameDataContext data;
    FillEffectReferenceContext(data);
    data.buildingRegistry->Load(buildingsPath.string());

    SECTION("known GrantBuilding on a probe action does not throw")
    {
        ProbeActionConfig_t action;
        action.id = ProbeActionId_t::Infiltrate;
        action.effects = {GrantBuilding("granted_hall")};
        data.probeActionsConfig->actions = {action};

        CHECK_NOTHROW(ValidateEffectReferences(data));
    }

    SECTION("unknown GrantBuilding throws naming the probe action")
    {
        ProbeActionConfig_t action;
        action.id = ProbeActionId_t::Infiltrate;
        action.effects = {GrantBuilding("no_such_building")};
        data.probeActionsConfig->actions = {action};

        CHECK_THROWS_WITH(ValidateEffectReferences(data),
                          Catch::Matchers::ContainsSubstring("probe_action:Infiltrate")
                              && Catch::Matchers::ContainsSubstring("no_such_building"));
    }

    std::filesystem::remove(buildingsPath);
}

TEST_CASE("ValidateEffectReferences(GameDataContext): production effect ids are checked",
          "[effects][validation][production]")
{
    const std::filesystem::path buildingsPath =
        actest::TestTempDir::Path() / "ac_production_effect_buildings.json";
    {
        std::ofstream out(buildingsPath);
        out << R"([
            { "id": "granted_hall", "name": "Granted Hall", "mineral_cost": 10, "category": "grow" }
        ])";
    }

    GameDataContext data;
    FillEffectReferenceContext(data);
    data.buildingRegistry->Load(buildingsPath.string());

    SECTION("known GrantBuilding on production does not throw")
    {
        data.productionConfig->effects = {GrantBuilding("granted_hall")};
        CHECK_NOTHROW(ValidateEffectReferences(data));
    }

    SECTION("unknown GrantBuilding throws naming production")
    {
        data.productionConfig->effects = {GrantBuilding("no_such_building")};
        CHECK_THROWS_WITH(ValidateEffectReferences(data),
                          Catch::Matchers::ContainsSubstring("production")
                              && Catch::Matchers::ContainsSubstring("no_such_building"));
    }

    std::filesystem::remove(buildingsPath);
}
