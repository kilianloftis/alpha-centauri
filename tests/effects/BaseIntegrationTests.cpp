// Integration tests through real BaseManager instances: base-scoped effect attribution,
// FilterForBase, triggered-effect dispatch, and the full resource-production pipeline.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/buildings/BuildingConfig.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/Military.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/production/ProductionApplyResult.h"
#include "game/map/Tile.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

using namespace ac;
using Catch::Approx;

TEST_CASE("Founding a base registers the Base improvement and its defense bonus on the tile",
          "[effects][base]")
{
    actest::BaseFixture fixture;
    fixture.MakeBase(4, 4);

    Tile& tile = fixture.At(4, 4);
    CHECK(tile.HasImprovement("Base"));
    CHECK(tile.HasFeature("Base"));
    // Fixture Base entry: +100% defense through the same mechanism as Bunker/Rocky.
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(tile, /*forFaction*/ 1) == Approx(2.0));
}

TEST_CASE("BaseManager::CollectBuildingEffects tags ThisBase effects with the owning base",
          "[effects][base]")
{
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);
    BaseManager& baseB = fixture.MakeBase(6, 6);

    baseA.GetBuildingManager().AddBuilding("flat_nutrient");
    baseB.GetBuildingManager().AddBuilding("granted_hall");

    const auto effectsA = baseA.CollectBuildingEffects();
    REQUIRE(effectsA.size() == 1);
    CHECK(effectsA[0].sourceId == "flat_nutrient");
    CHECK(effectsA[0].originBase == &baseA);

    // granted_hall: ThisBase minerals -> tagged with baseB; FactionGlobal energy -> untagged.
    const auto effectsB = baseB.CollectBuildingEffects();
    REQUIRE(effectsB.size() == 2);
    for (const ActiveEffect_t& effect : effectsB)
    {
        if (effect.config->scope == EffectScope_t::ThisBase)
        {
            CHECK(effect.originBase == &baseB);
        }
        else
        {
            CHECK(effect.originBase == nullptr);
        }
    }
}

TEST_CASE("FilterForBase: scope rules with real base identities", "[effects][base][filter]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& baseA = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& baseB = fixture.MakeFactionBase(faction, 6, 6);

    actest::EffectPool pool;
    const FactionEffects_t factionEffects{faction, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase),
                       "mine_a", &baseA),
        actest::Active(pool.StatMod(StatId_t::Nutrients, 2.0, ModifierOp_t::Add, EffectScope_t::ThisBase),
                       "theirs_b", &baseB),
        actest::Active(pool.StatMod(StatId_t::Nutrients, 4.0, ModifierOp_t::Add, EffectScope_t::AllOwnerBases),
                       "all_bases"),
        actest::Active(pool.StatMod(StatId_t::Nutrients, 8.0, ModifierOp_t::Add, EffectScope_t::FactionGlobal),
                       "faction"),
        actest::Active(pool.StatMod(StatId_t::Nutrients, 16.0, ModifierOp_t::Add, EffectScope_t::WorldGlobal),
                       "world"),
        actest::Active(pool.StatMod(StatId_t::Nutrients, 32.0, ModifierOp_t::Add, EffectScope_t::ThisUnit),
                       "unit"),
        actest::Active(pool.StatMod(StatId_t::Nutrients, 64.0, ModifierOp_t::Add, EffectScope_t::FactionUnits),
                       "faction_units"),
        actest::Active(pool.StatMod(StatId_t::Nutrients, 128.0, ModifierOp_t::Add, EffectScope_t::ThisPop),
                       "pop"),
        actest::Active(pool.StatMod(StatId_t::Nutrients, 256.0, ModifierOp_t::Add, EffectScope_t::ThisTile),
                       "tile"),
    }};

    // For base A: its own ThisBase effect plus the three base-applicable global scopes.
    // The distinct powers of two make any wrong inclusion identifiable from the total.
    const BaseEffects_t forA = FilterForBase(factionEffects, baseA);
    CHECK(ResolveStatModifiers(FilterByStatId(forA.effects, StatId_t::Nutrients), 0.0).total == Approx(1.0 + 4.0 + 8.0 + 16.0));

    const BaseEffects_t forB = FilterForBase(factionEffects, baseB);
    CHECK(ResolveStatModifiers(FilterByStatId(forB.effects, StatId_t::Nutrients), 0.0).total == Approx(2.0 + 4.0 + 8.0 + 16.0));
}

TEST_CASE("FilterForBase: a ThisBase effect with no origin base applies to no base", "[effects][base][filter]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& baseA = fixture.MakeFactionBase(faction, 2, 2);

    actest::EffectPool pool;
    const FactionEffects_t factionEffects{faction, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase),
                       "orphan", nullptr),
    }};
    CHECK(FilterForBase(factionEffects, baseA).effects.empty());
}

TEST_CASE("ApplyTriggeredEffects: AddBuilding constructs the building immediately",
          "[effects][base][triggered]")
{
    actest::FactionFixture fixture;
    GameSettings settings;
    auto pMap = std::make_unique<WorldMap>(9, 9, actest::TestMapRules());
    GameState state(std::move(pMap), fixture.improvements, &fixture.unitComponents, settings,
                    fixture.morale(), fixture.dataContext.tileYieldRules,
                    fixture.dataContext.interactionGrids,
                    actest::k_TestRngSeed);
    Faction& faction = state.AddFaction(std::make_unique<Faction>(
        state.AllocateFactionId(), true, fixture.factionDefinition, fixture.dataContext,
        state.GetWorldMap(), fixture.settings, actest::k_TestFactionSeed));
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    const BuildingConfig_t* pGrantor = fixture.buildings().Find("instant_grantor");
    REQUIRE(pGrantor != nullptr);

    // Simulate OnProductionCompleted for instant_grantor.
    base.GetBuildingManager().AddBuilding(pGrantor->id);
    TriggeredEffectContext_t context(state, base);
    ApplyTriggeredEffects(pGrantor->onCompleteEffects, context);

    bool hasGranted = false;
    for (const BuildingConfig_t* pBuilding : base.GetBuildingManager().GetBuildings())
    {
        if (pBuilding->id == "flat_nutrient")
        {
            hasGranted = true;
        }
    }
    CHECK(hasGranted);

    // The granted building is a real constructed building: its continuous effects collect.
    const auto effects = base.CollectBuildingEffects();
    CHECK(ResolveStatModifiers(FilterByStatId(effects, StatId_t::Nutrients), 0.0).total == 2.0);
}

// End-to-end for the whole dispatch path: GameState::AddFaction binds the session, so
// completing production reaches the SetInfiltration arm without the caller passing anything.
// Drives TryCompleteReadyProduction rather than ApplyTriggeredEffects so that dropping either
// BindGameState or the dispatch call in BaseManager's completion handler fails here.
TEST_CASE("Production completion writes on_complete SetInfiltration into the DiplomacyLedger",
          "[effects][base][triggered][infiltration]")
{
    actest::FactionFixture fixture;
    GameSettings settings;
    auto pMap = std::make_unique<WorldMap>(9, 9, actest::TestMapRules());
    GameState state(std::move(pMap), fixture.improvements, &fixture.unitComponents, settings,
                    fixture.morale(), fixture.dataContext.tileYieldRules,
                    fixture.dataContext.interactionGrids,
                    actest::k_TestRngSeed);
    Faction& beneficiary = state.AddFaction(std::make_unique<Faction>(
        state.AllocateFactionId(), true, fixture.factionDefinition, fixture.dataContext,
        state.GetWorldMap(), fixture.settings, actest::k_TestFactionSeed));
    Faction& other = state.AddFaction(std::make_unique<Faction>(
        state.AllocateFactionId(), false, fixture.factionDefinition, fixture.dataContext,
        state.GetWorldMap(), fixture.settings, actest::k_TestFactionSeed));
    BaseManager& base = fixture.MakeFactionBase(beneficiary, 4, 4);

    const BuildingConfig_t* pInfiltrator = fixture.buildings().Find("instant_infiltrator");
    REQUIRE(pInfiltrator != nullptr);
    REQUIRE(beneficiary.GetGameState() == &state);
    REQUIRE_FALSE(state.GetDiplomacyLedger().HasInfiltration(
        beneficiary.GetFactionId(), other.GetFactionId()));

    base.GetProduction().SetProduction(pInfiltrator, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());
    REQUIRE(base.TryCompleteReadyProduction().kind == ProductionApplyKind_t::Completed);

    CHECK(state.GetDiplomacyLedger().HasInfiltration(
        beneficiary.GetFactionId(), other.GetFactionId()));
    // Ledger forbids self-pairs; FactionFilterCoversTarget also excludes the beneficiary.
}

// Building completion no longer needs session spawn services; on_complete effects only run
// when a session subscriber is wired (GameState::AttachToSession_).
TEST_CASE("Building production without spawn services completes without on_complete effects",
          "[effects][base][triggered]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    REQUIRE(fixture.pOwnerFaction->GetGameState() == nullptr);

    const BuildingConfig_t* pGrantor = fixture.buildings().Find("instant_grantor");
    REQUIRE(pGrantor != nullptr);
    base.GetProduction().SetProduction(pGrantor, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());
    REQUIRE(base.TryCompleteReadyProduction().kind == ProductionApplyKind_t::Completed);

    bool hasGrantor = false;
    bool hasGranted = false;
    for (const BuildingConfig_t* pBuilding : base.GetBuildingManager().GetBuildings())
    {
        if (pBuilding->id == "instant_grantor")
        {
            hasGrantor = true;
        }
        if (pBuilding->id == "flat_nutrient")
        {
            hasGranted = true;
        }
    }
    CHECK(hasGrantor);
    CHECK_FALSE(hasGranted);
}

TEST_CASE("Unit production without spawn services throws before spawning",
          "[effects][base][triggered]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    UnitSlotConfig_t chassisSlot;
    chassisSlot.id = "slot_0";
    chassisSlot.displayName = "slot_0";
    chassisSlot.componentType = "chassis";
    chassisSlot.required = true;
    UnitSlotConfig_t weaponSlot;
    weaponSlot.id = "slot_1";
    weaponSlot.displayName = "slot_1";
    weaponSlot.componentType = "weapon";
    weaponSlot.required = true;
    const UnitComponentConfig_t* pChassis = fixture.unitComponents.Find("test_chassis");
    const UnitComponentConfig_t* pWeapon = fixture.unitComponents.Find("test_weapon");
    REQUIRE(pChassis != nullptr);
    REQUIRE(pWeapon != nullptr);
    std::unordered_map<std::string, const UnitComponentConfig_t*> assigned{
        {chassisSlot.id, pChassis},
        {weaponSlot.id, pWeapon},
    };
    auto pDesign =
        std::make_unique<UnitDesign>(std::vector{chassisSlot, weaponSlot}, assigned);
    REQUIRE(faction.GetMilitary().AddDesign(std::move(pDesign)));
    const UnitDesign* pQueued =
        dynamic_cast<const UnitDesign*>(faction.GetMilitary().GetDesigns().front().get());
    REQUIRE(pQueued != nullptr);

    base.GetProduction().SetProduction(pQueued, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());
    CHECK_THROWS_AS(base.TryCompleteReadyProduction(), std::runtime_error);
    CHECK(faction.GetUnitManager().Units().empty());
}

TEST_CASE("Full pipeline: building and pop bonuses land in base resource production",
          "[effects][base][pipeline]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    // A wet farm tile next door: Wet(+2) + Farm(+1) nutrients.
    Tile& farmTile = fixture.At(5, 4);
    farmTile.SetBaseMoisture(Moisture_t::Wet);
    farmTile.SetMoisture(Moisture_t::Wet);
    fixture.ctx->AddImprovementWithEffects(farmTile, "Farm");

    // Buildings: +2 flat nutrients, +1 nutrients on each worked Farm.
    base.GetBuildingManager().AddBuilding("flat_nutrient");
    base.GetBuildingManager().AddBuilding("farm_booster");

    // One of the three starting Workers works the farm tile; another becomes a Doctor
    // (+2 psych). Workers are auto-assigned at base construction, so pick any pop that is
    // not working the farm — the tiles the others work are all barren (zero yield).
    base.UserAssignBestAvailableWorker(&farmTile);
    PopulationManager& rPopulation = base.GetPopulation();
    REQUIRE(rPopulation.GetSize() == 3);
    Pop* pOtherWorker = nullptr;
    for (Pop& rPop : rPopulation.Pops())
    {
        if (rPop.GetTile() != &farmTile)
        {
            pOtherWorker = &rPop;
            break;
        }
    }
    REQUIRE(pOtherWorker != nullptr);
    base.ConvertPop(*pOtherWorker, "Doctor");

    // Nutrients: farm tile (2 Wet + 1 Farm + 1 booster) is capped at 2 until gene_splicing,
    // then + flat 2 from the building = 4. Discovering the tech lifts the tile to 4 → 6 total.
    CHECK(base.GetNutrientProduction() == 4);

    faction.GetResearch().AddDiscoveredTech("gene_splicing");
    CHECK(base.GetNutrientProduction() == 6);
    // Minerals: nothing anywhere.
    CHECK(base.GetMineralProduction() == 0);
    // Psych: no energy to split (all elevation 0), so exactly the Doctor's +2.
    CHECK(base.GetPsychProduction() == 2);
}
