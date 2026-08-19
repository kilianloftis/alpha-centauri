// Stockpile production items convert minerals forever via MineralsConverted StatModifiers
// into the base's nutrient / econ / labs / psych banks. The highest-priority available
// stockpile (tech gate, then fallback_priority) is the empty-queue default; if none is
// available, minerals are wasted.

#include "GameFixtures.h"

#include "game/IConstructable.h"
#include "game/Faction.h"
#include "game/HookContext.h"
#include "game/TurnProcessor.h"
#include "game/TurnStages.h"
#include "game/faction/EconomyManager.h"
#include "game/stages/IncomeCollection.h"
#include "game/stages/ResourceCollection.h"
#include "game/stages/MineralConversion.h"
#include "game/stages/UnitSupport.h"
#include "game/faction/ResearchManager.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/production/ProductionApplyResult.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/map/WorldMap.h"
#include "game/stockpiles/StockpileConfig.h"
#include "game/stockpiles/StockpileRegistry.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

class AlwaysYieldStage_ : public GlobalTurnStage
{
public:
    using GlobalTurnStage::GlobalTurnStage;
protected:
    StageResult_t ExecuteImpl(GameState&) override { return StageResult_t::Yield; }
};

void LeaveMineralBank_(BaseManager& rBase, int desired)
{
    if (!rBase.GetBuildingManager().HasBuilding("mineral_cache"))
    {
        rBase.GetBuildingManager().AddBuilding("mineral_cache");
    }
    rBase.ProduceResources();
    ResourceManager& rResources = rBase.GetResources();
    const int bank = rResources.GetMineralBank();
    REQUIRE(bank >= desired);
    rResources.SpendMinerals(bank - desired);
    REQUIRE(rResources.GetMineralBank() == desired);
}

std::string WriteTempStockpiles_(const std::string& rContents)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_stockpiles.json";
    std::ofstream out(path);
    out << rContents;
    return path.string();
}

std::string MineralsConverted_(const char* pStat, const char* pAmount)
{
    return std::string(R"({ "type": "StatModifier", "scope": "ThisBase",
        "parameters": { "stat": ")")
        + pStat + R"(", "amount": )" + pAmount
        + R"(, "amount_source": "MineralsConverted" } })";
}

std::string StockpileJson_(const char* pId, const char* pName, const char* pStat,
                           const char* pAmount, const char* pRounding = "down",
                           const char* pExtraFields = "")
{
    return std::string(R"({ "id": ")") + pId + R"(", "name": ")" + pName
        + R"(", "rounding": ")" + pRounding + R"(")" + pExtraFields + R"(,
        "effects": [)" + MineralsConverted_(pStat, pAmount) + R"(] })";
}

BaseManager& MakeBaseWithStockpiles_(FactionFixture& rFixtures, Faction& rFaction,
                                     StockpileRegistry& rStockpiles)
{
    auto pBase = std::make_unique<BaseManager>(
        rFaction, rFixtures.nextBaseId++, "TestBase", rFixtures.At(4, 4),
        *rFixtures.dataContext.buildingRegistry,
        rStockpiles,
        *rFixtures.dataContext.socialRatingRegistry,
        *rFixtures.dataContext.popTypeRegistry,
        *rFixtures.dataContext.popTypeAvailabilityCalculator,
        *rFixtures.dataContext.growthConfig,
        *rFixtures.dataContext.productionConfig,
        *rFixtures.dataContext.hurryProductionCalculator,
        *rFixtures.dataContext.scrapRefundCalculator,
        *rFixtures.dataContext.droneCalculator,
        *rFixtures.dataContext.popCompositionCalculator,
        nullptr,
        *rFixtures.ctx);
    BaseManager& rBase = *pBase;
    rFaction.AddBase(std::move(pBase));
    return rBase;
}

BaseManager& MakeBaseWith_(FactionFixture& rFixtures, Faction& rFaction,
                           BuildingRegistry& rBuildings, StockpileRegistry& rStockpiles)
{
    auto pBase = std::make_unique<BaseManager>(
        rFaction, rFixtures.nextBaseId++, "TestBase", rFixtures.At(4, 4),
        rBuildings,
        rStockpiles,
        *rFixtures.dataContext.socialRatingRegistry,
        *rFixtures.dataContext.popTypeRegistry,
        *rFixtures.dataContext.popTypeAvailabilityCalculator,
        *rFixtures.dataContext.growthConfig,
        *rFixtures.dataContext.productionConfig,
        *rFixtures.dataContext.hurryProductionCalculator,
        *rFixtures.dataContext.scrapRefundCalculator,
        *rFixtures.dataContext.droneCalculator,
        *rFixtures.dataContext.popCompositionCalculator,
        nullptr,
        *rFixtures.ctx);
    BaseManager& rBase = *pBase;
    rFaction.AddBase(std::move(pBase));
    return rBase;
}

std::string WriteTempBuildings_(const std::string& rContents)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_stockpile_buildings.json";
    std::ofstream out(path);
    out << rContents;
    return path.string();
}

// Econ is the stock stockpile output: it reaches the treasury via IncomeCollection, which
// turn_stages.json orders after MineralConversion.
int TakeEcon_(BaseManager& rBase)
{
    return rBase.GetResources().ConsumeEcon();
}

void DrainEnergyBanks_(BaseManager& rBase)
{
    (void)rBase.GetResources().ConsumeEcon();
    (void)rBase.GetResources().ConsumeLabs();
    (void)rBase.GetResources().ConsumePsych();
}

} // namespace

TEST_CASE("A new base queues the default stockpile", "[production][stockpile]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);

    const StockpileConfig_t* pStockpile = fixtures.stockpiles().Find("Stockpile_Energy");
    REQUIRE(pStockpile != nullptr);
    CHECK(base.GetProduction().GetCurrentProduction() == pStockpile);
    CHECK(base.GetMineralCost() == 0);
    CHECK_FALSE(base.GetProduction().IsReadyToComplete(base.GetBaseEffects(), false));
}

TEST_CASE("Clearing production falls back to the default stockpile", "[production][stockpile]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);
    const StockpileConfig_t* pStockpile = fixtures.stockpiles().Find("Stockpile_Energy");
    REQUIRE(pStockpile != nullptr);
    const BuildingConfig_t* pFacility = fixtures.buildings().Find("test_facility_a");
    REQUIRE(pFacility != nullptr);

    base.GetProduction().SetProduction(pFacility);
    REQUIRE(base.GetProduction().GetCurrentProduction() == pFacility);

    base.GetProduction().SetProduction(nullptr);
    CHECK(base.GetProduction().GetCurrentProduction() == pStockpile);
}

// Falling back is not a player choice, so it must not charge the retool forfeit — otherwise
// a base that idles on the default loses banked minerals the moment the player picks a build.
TEST_CASE("Falling back to the default never charges the retool penalty",
          "[production][stockpile][retool]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);
    const BuildingConfig_t* pFacility = fixtures.buildings().Find("test_facility_a");
    REQUIRE(pFacility != nullptr);

    base.GetProduction().SetProduction(pFacility);
    base.GetProduction().SetMineralStockpile(40);

    base.GetProduction().SetProduction(nullptr);
    CHECK(base.GetProduction().GetCurrentProduction()->GetId() == "Stockpile_Energy");
    CHECK(base.GetProduction().GetMineralStockpile() == 40);
}

TEST_CASE("Stockpile Energy converts this turn's minerals and never completes",
          "[production][stockpile]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);
    const StockpileConfig_t* pStockpile = fixtures.stockpiles().Find("Stockpile_Energy");
    REQUIRE(pStockpile != nullptr);
    REQUIRE(base.GetProduction().GetCurrentProduction() == pStockpile);

    base.GetProduction().SetMineralStockpile(7);
    LeaveMineralBank_(base, 5);
    (void)TakeEcon_(base);

    base.ConvertMinerals();
    // ceil(5 * 0.5) = 3 — the stock rounding is "up", so an odd mineral favours the player.
    CHECK(TakeEcon_(base) == 3);
    CHECK(base.GetResources().GetMineralBank() == 0);

    const ProductionApplyResult_t applied = base.ApplyProduction();
    CHECK(applied.kind == ProductionApplyKind_t::InProgress);
    CHECK(base.GetProduction().GetCurrentProduction() == pStockpile);
    CHECK(base.GetProduction().GetMineralStockpile() == 7);
    CHECK_FALSE(base.GetProduction().IsReadyToComplete(base.GetBaseEffects(), false));
}

TEST_CASE("Stockpile rounding is taken from config, not assumed", "[production][stockpile]")
{
    auto convertWith = [](const char* pRounding, int minerals) {
        StockpileRegistry registry;
        registry.Load(WriteTempStockpiles_(
            std::string("[") + StockpileJson_("quarter", "Quarter", "econ", "0.25", pRounding)
            + "]"));

        FactionFixture fixtures;
        Faction& faction = fixtures.MakeFaction();
        BaseManager& base = MakeBaseWithStockpiles_(fixtures, faction, registry);
        LeaveMineralBank_(base, minerals);
        (void)TakeEcon_(base);
        base.ConvertMinerals();
        return TakeEcon_(base);
    };

    SECTION("down")
    {
        CHECK(convertWith("down", 0) == 0);
        CHECK(convertWith("down", 1) == 0); // 0.25
        CHECK(convertWith("down", 4) == 1);
        CHECK(convertWith("down", 7) == 1); // 1.75
    }
    SECTION("up")
    {
        CHECK(convertWith("up", 1) == 1);
        CHECK(convertWith("up", 5) == 2); // 1.25
    }
    SECTION("nearest")
    {
        CHECK(convertWith("nearest", 5) == 1); // 1.25
        CHECK(convertWith("nearest", 7) == 2); // 1.75
    }
}

TEST_CASE("A stockpile converts zero minerals to zero output", "[production][stockpile]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);

    LeaveMineralBank_(base, 0);
    (void)TakeEcon_(base);

    base.ConvertMinerals();
    CHECK(TakeEcon_(base) == 0);
    CHECK(base.ApplyProduction().kind == ProductionApplyKind_t::InProgress);
}

// A stockpile is not a building at all now, so the building registry simply does not know
// the id — there is no "is this a stockpile?" branch left in BuildingManager to get wrong.
TEST_CASE("A stockpile is not a building", "[production][stockpile]")
{
    FactionFixture fixtures;

    CHECK(fixtures.buildings().Find("Stockpile_Energy") == nullptr);
    REQUIRE(fixtures.stockpiles().Find("Stockpile_Energy") != nullptr);

    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);
    CHECK_THROWS_AS(base.GetBuildingManager().CanAddBuilding("Stockpile_Energy"),
                    std::runtime_error);
    CHECK_FALSE(base.GetBuildingManager().HasBuilding("Stockpile_Energy"));
}

TEST_CASE("Stockpile Energy appears in the constructable list", "[production][stockpile]")
{
    FactionFixture fixtures;
    GameSettings settings;
    auto pMap = std::make_unique<WorldMap>(9, 9);
    for (auto& pTile : pMap->GetTiles())
    {
        pTile->SetElevation(100);
    }
    auto pState = std::make_unique<GameState>(
        std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
        *fixtures.dataContext.moraleCalculator, k_TestRngSeed);
    Faction& faction = pState->AddFaction(std::make_unique<Faction>(
        pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
        pState->GetWorldMap(), fixtures.settings, k_TestFactionSeed));
    BaseManager* pBase = faction.CreateBase(
        pState->AllocateBaseId(), "TestBase", pState->GetWorldMap().GetTile(2, 2),
        fixtures.dataContext, pState->GetTileEffects(),
        pState->GetSecretProjectAvailability());
    REQUIRE(pBase != nullptr);

    const StockpileConfig_t* pStockpile = fixtures.stockpiles().Find("Stockpile_Energy");
    REQUIRE(pStockpile != nullptr);

    const std::vector<const IConstructable*> available = pBase->GetConstructable();
    CHECK(std::find(available.begin(), available.end(), pStockpile) != available.end());
}

TEST_CASE("Percentage modifiers on the stockpile scale conversion yield",
          "[production][stockpile]")
{
    StockpileRegistry registry;
    registry.Load(WriteTempStockpiles_(R"([
        { "id": "boosted", "name": "Boosted", "rounding": "down",
          "effects": [
            { "type": "StatModifier", "scope": "ThisBase",
              "parameters": { "stat": "econ", "amount": 0.5,
                              "amount_source": "MineralsConverted" } },
            { "type": "StatModifier", "scope": "ThisBase",
              "parameters": { "stat": "econ", "amount": 100, "op": "AddPercent" } }
          ] }
    ])"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = MakeBaseWithStockpiles_(fixtures, faction, registry);
    REQUIRE(base.GetProduction().GetCurrentProduction() != nullptr);
    CHECK(base.GetProduction().GetCurrentProduction()->GetId() == "boosted");

    LeaveMineralBank_(base, 5);
    (void)TakeEcon_(base);
    base.ConvertMinerals();
    // 5 * 0.5 = 2.5, AddPercent 100 -> 5.0
    CHECK(TakeEcon_(base) == 5);
}

// "energy" is not a bank. A stockpile producing it must take the same route collected tile
// energy takes — inefficiency, then the econ/labs/psych sliders — rather than landing in one
// bank. Allocating everything to labs makes the difference visible: a direct credit would
// show up in econ.
TEST_CASE("Converted energy goes through the slider split", "[production][stockpile][energy]")
{
    StockpileRegistry registry;
    registry.Load(WriteTempStockpiles_(
        std::string("[") + StockpileJson_("raw_energy", "Raw", "energy", "1") + "]"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    faction.GetEconomy().SetEnergyAllocation({0, 100, 0});
    BaseManager& base = MakeBaseWithStockpiles_(fixtures, faction, registry);

    LeaveMineralBank_(base, 10);
    DrainEnergyBanks_(base);
    base.ConvertMinerals();

    CHECK(base.GetResources().ConsumeLabs() > 0);
    CHECK(base.GetResources().ConsumeEcon() == 0);
    CHECK(base.GetResources().ConsumePsych() == 0);
}

// Energy conversion reuses only the faction's split math, never CalculateEcon_ / Labs_ /
// Psych_ — those re-seed the split with the base's flat Econ/Labs/Psych StatModifiers, which
// ProduceResources already applied this turn. Converting the same minerals at the same base
// before and after gaining a flat +econ facility must give the same conversion yield; if it
// grows, the facility's bonus is being paid twice a turn.
TEST_CASE("Converted energy does not re-apply flat econ modifiers",
          "[production][stockpile][energy]")
{
    BuildingRegistry buildings;
    buildings.Load(WriteTempBuildings_(R"([
        { "id": "mineral_cache", "name": "Mineral Cache",
          "effects": [{
            "type": "StatModifier", "scope": "ThisBase",
            "parameters": { "stat": "minerals", "amount": 50, "op": "Add" }
          }] },
        { "id": "econ_hall", "name": "Econ Hall",
          "effects": [{
            "type": "StatModifier", "scope": "ThisBase",
            "parameters": { "stat": "econ", "amount": 7, "op": "Add" }
          }] }
    ])"));

    StockpileRegistry stockpiles;
    stockpiles.Load(WriteTempStockpiles_(
        std::string("[") + StockpileJson_("raw_energy", "Raw", "energy", "1") + "]"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    faction.GetEconomy().SetEnergyAllocation({100, 0, 0});
    BaseManager& base = MakeBaseWith_(fixtures, faction, buildings, stockpiles);

    LeaveMineralBank_(base, 10);
    const int collectedWithout = TakeEcon_(base);
    DrainEnergyBanks_(base);
    base.ConvertMinerals();
    const int convertedWithout = TakeEcon_(base);
    REQUIRE(convertedWithout > 0);

    base.GetBuildingManager().AddBuilding("econ_hall");
    LeaveMineralBank_(base, 10);
    // The facility is live — collection pays its flat +7 once...
    REQUIRE(TakeEcon_(base) == collectedWithout + 7);
    DrainEnergyBanks_(base);
    base.ConvertMinerals();
    // ...and conversion must not pay it again.
    CHECK(TakeEcon_(base) == convertedWithout);
}

TEST_CASE("A nutrient stockpile credits the nutrient bank", "[production][stockpile]")
{
    StockpileRegistry registry;
    registry.Load(WriteTempStockpiles_(
        std::string("[") + StockpileJson_("stock_nutrients", "Nutrients", "nutrients", "1")
        + "]"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = MakeBaseWithStockpiles_(fixtures, faction, registry);

    LeaveMineralBank_(base, 4);
    (void)base.GetResources().ConsumeNutrients();
    base.ConvertMinerals();
    CHECK(base.GetResources().ConsumeNutrients() == 4);
}

TEST_CASE("A stockpile can credit more than one output stat", "[production][stockpile]")
{
    StockpileRegistry registry;
    registry.Load(WriteTempStockpiles_(R"([
        { "id": "split", "name": "Split", "rounding": "down",
          "effects": [
            { "type": "StatModifier", "scope": "ThisBase",
              "parameters": { "stat": "econ", "amount": 0.5,
                              "amount_source": "MineralsConverted" } },
            { "type": "StatModifier", "scope": "ThisBase",
              "parameters": { "stat": "nutrients", "amount": 0.25,
                              "amount_source": "MineralsConverted" } }
          ] }
    ])"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = MakeBaseWithStockpiles_(fixtures, faction, registry);

    LeaveMineralBank_(base, 4);
    (void)base.GetResources().ConsumeNutrients();
    (void)TakeEcon_(base);
    base.ConvertMinerals();
    CHECK(TakeEcon_(base) == 2);                       // floor(4 * 0.5)
    CHECK(base.GetResources().ConsumeNutrients() == 1); // floor(4 * 0.25)
}

TEST_CASE("The default skips a tech-gated stockpile until the tech is discovered",
          "[production][stockpile]")
{
    StockpileRegistry registry;
    registry.Load(WriteTempStockpiles_(
        std::string("[")
        + StockpileJson_("gated", "Gated", "labs", "1", "down",
                         R"(, "required_tech": "advanced_build", "fallback_priority": 10)")
        + "," + StockpileJson_("open", "Open", "econ", "0.5") + "]"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = MakeBaseWithStockpiles_(fixtures, faction, registry);
    CHECK(base.GetProduction().GetCurrentProduction()->GetId() == "open");

    faction.GetResearch().AddDiscoveredTech("advanced_build");
    base.GetProduction().SetProduction(nullptr);
    CHECK(base.GetProduction().GetCurrentProduction()->GetId() == "gated");
}

// fallback_priority, not file order, decides the default — a mod dropping in another
// stockpile file must not silently change what every base builds.
TEST_CASE("The default is the highest fallback_priority, not the first loaded",
          "[production][stockpile]")
{
    StockpileRegistry registry;
    registry.Load(WriteTempStockpiles_(
        std::string("[") + StockpileJson_("first", "First", "econ", "0.5")
        + "," + StockpileJson_("preferred", "Preferred", "labs", "1", "down",
                               R"(, "fallback_priority": 5)")
        + "]"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = MakeBaseWithStockpiles_(fixtures, faction, registry);
    CHECK(base.GetProduction().GetCurrentProduction()->GetId() == "preferred");
}

TEST_CASE("With no available stockpile, the queue stays empty and minerals are wasted",
          "[production][stockpile]")
{
    StockpileRegistry registry;
    registry.Load(WriteTempStockpiles_(
        std::string("[")
        + StockpileJson_("gated", "Gated", "econ", "0.5", "down",
                         R"(, "required_tech": "advanced_build")")
        + "]"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = MakeBaseWithStockpiles_(fixtures, faction, registry);
    CHECK_FALSE(base.GetProduction().HasProduction());

    LeaveMineralBank_(base, 5);
    (void)TakeEcon_(base);
    base.ConvertMinerals();
    CHECK_FALSE(base.GetProduction().HasProduction());
    CHECK(base.GetResources().GetMineralBank() == 0);
    CHECK(TakeEcon_(base) == 0);
    CHECK(base.ApplyProduction().kind == ProductionApplyKind_t::Idle);
}

// MineralConversion is the leftover-bank drain. ApplyProduction on an idle base must not be
// a second waste path, or minerals filled after that stage vanish instead of converting next
// turn.
TEST_CASE("ApplyProduction on an empty queue leaves the mineral bank", "[production][stockpile]")
{
    StockpileRegistry registry;
    registry.Load(WriteTempStockpiles_(
        std::string("[")
        + StockpileJson_("gated", "Gated", "econ", "0.5", "down",
                         R"(, "required_tech": "advanced_build")")
        + "]"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = MakeBaseWithStockpiles_(fixtures, faction, registry);
    CHECK_FALSE(base.GetProduction().HasProduction());

    LeaveMineralBank_(base, 5);
    REQUIRE(base.ApplyProduction().kind == ProductionApplyKind_t::Idle);
    CHECK(base.GetResources().GetMineralBank() == 5);
}

// Conversion belongs to MineralConversion, which runs before IncomeCollection and
// ResearchAccumulation. ApplyProduction must not convert as a side effect, or econ and labs
// credited there would arrive after those stages had already drained the banks.
TEST_CASE("ApplyProduction does not convert surplus minerals", "[production][stockpile]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);

    LeaveMineralBank_(base, 5);
    (void)TakeEcon_(base);
    REQUIRE(base.ApplyProduction().kind == ProductionApplyKind_t::InProgress);
    CHECK(TakeEcon_(base) == 0);
    CHECK(base.GetResources().GetMineralBank() == 5);
}

TEST_CASE("Mineral support claims the bank before stockpile conversion",
          "[production][stockpile][support]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);
    // Support 0 grants two free slots; the third chassis costs 1 mineral.
    fixtures.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    fixtures.MakeUnit(faction, 6, 4, {"test_chassis"}, &base);
    fixtures.MakeUnit(faction, 7, 4, {"test_chassis"}, &base);

    LeaveMineralBank_(base, 5);
    (void)TakeEcon_(base);
    base.ApplyMineralSupport();
    CHECK(base.GetResources().GetMineralBank() == 4);

    base.ConvertMinerals();
    // ceil(4 * 0.5) = 2
    CHECK(TakeEcon_(base) == 2);
    CHECK(base.GetResources().GetMineralBank() == 0);
}

TEST_CASE("Stockpile econ reaches the treasury the same turn", "[production][stockpile]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);

    LeaveMineralBank_(base, 8);
    (void)TakeEcon_(base);
    base.ConvertMinerals();
    CHECK(faction.CollectIncome() == 4); // ceil(8 * 0.5)
}

// Every other test here calls ConvertMinerals directly, so none of them would notice
// if turn_stages.json ordered MineralConversion after IncomeCollection. This one runs the
// real stage sequence: converted econ only reaches the treasury this turn because
// MineralConversion is ordered ahead of IncomeCollection.
TEST_CASE("The stage sequence converts and banks surplus in the same turn",
          "[production][stockpile][stages]")
{
    FactionFixture fixtures;
    GameSettings settings;
    auto pMap = std::make_unique<WorldMap>(9, 9);
    for (auto& pTile : pMap->GetTiles())
    {
        pTile->SetElevation(100);
    }
    auto pState = std::make_unique<GameState>(
        std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
        *fixtures.dataContext.moraleCalculator, k_TestRngSeed);
    Faction& faction = pState->AddFaction(std::make_unique<Faction>(
        pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
        pState->GetWorldMap(), fixtures.settings, k_TestFactionSeed));
    BaseManager* pBase = faction.CreateBase(
        pState->AllocateBaseId(), "TestBase", pState->GetWorldMap().GetTile(4, 4),
        fixtures.dataContext, pState->GetTileEffects(),
        pState->GetSecretProjectAvailability());
    REQUIRE(pBase != nullptr);
    REQUIRE(pBase->GetProduction().GetCurrentProduction()->GetId() == "Stockpile_Energy");
    pBase->GetBuildingManager().AddBuilding("mineral_cache");

    PerFactionTurnStageRegistry_t perFaction;
    perFaction["ResourceCollection"] = std::make_unique<ResourceCollection>(HookContext{});
    perFaction["UnitSupport"] = std::make_unique<UnitSupport>(HookContext{});
    perFaction["MineralConversion"] = std::make_unique<MineralConversion>(HookContext{});
    perFaction["IncomeCollection"] = std::make_unique<IncomeCollection>(HookContext{});
    GlobalTurnStageRegistry_t global;
    global["Stop"] = std::make_unique<AlwaysYieldStage_>(HookContext{});
    TurnProcessor processor(
        std::move(global), std::move(perFaction),
        {"ResourceCollection", "UnitSupport", "MineralConversion", "IncomeCollection", "Stop"});

    const int energyBefore = faction.GetEconomy().GetEnergy();
    const int mineralsPerTurn = pBase->GetMineralProduction();
    REQUIRE(mineralsPerTurn > 0);

    processor.Advance(*pState);

    // The whole bank converts (nothing else is queued) and lands in the treasury this turn.
    CHECK(pBase->GetResources().GetMineralBank() == 0);
    CHECK(faction.GetEconomy().GetEnergy() == energyBefore + (mineralsPerTurn + 1) / 2);
}

TEST_CASE("A base restored from a snapshot keeps its queued stockpile",
          "[production][stockpile][snapshot]")
{
    FactionFixture fixtures;
    GameSettings settings;
    auto pMap = std::make_unique<WorldMap>(9, 9);
    for (auto& pTile : pMap->GetTiles())
    {
        pTile->SetElevation(100);
    }
    auto pState = std::make_unique<GameState>(
        std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
        *fixtures.dataContext.moraleCalculator, k_TestRngSeed);
    Faction& faction = pState->AddFaction(std::make_unique<Faction>(
        pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
        pState->GetWorldMap(), fixtures.settings, k_TestFactionSeed));
    BaseManager* pBase = faction.CreateBase(
        pState->AllocateBaseId(), "TestBase", pState->GetWorldMap().GetTile(4, 4),
        fixtures.dataContext, pState->GetTileEffects(),
        pState->GetSecretProjectAvailability());
    REQUIRE(pBase != nullptr);
    pBase->GetProduction().SetMineralStockpile(12);

    const BaseSnapshot_t snapshot = pBase->CaptureSnapshot();
    CHECK(snapshot.productionItemId == "Stockpile_Energy");

    const std::optional<BaseSnapshot_t> extracted = faction.ExtractBase(pBase->GetBaseId());
    REQUIRE(extracted.has_value());
    BaseManager* pRestored = faction.CreateBaseFromSnapshot(
        *extracted, fixtures.dataContext, pState->GetTileEffects(),
        pState->GetSecretProjectAvailability());
    REQUIRE(pRestored != nullptr);

    // Resolved through the stockpile registry, not the building registry.
    REQUIRE(pRestored->GetProduction().GetCurrentProduction() != nullptr);
    CHECK(pRestored->GetProduction().GetCurrentProduction()
          == fixtures.stockpiles().Find("Stockpile_Energy"));
    CHECK(pRestored->GetProduction().GetMineralStockpile() == 12);
}

TEST_CASE("ConvertMinerals banks leftover minerals into a real production item",
          "[production][stockpile]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);
    const BuildingConfig_t* pFacility = fixtures.buildings().Find("test_facility_a");
    REQUIRE(pFacility != nullptr);

    base.GetProduction().SetProduction(pFacility);
    const int stockpileBefore = base.GetProduction().GetMineralStockpile();
    LeaveMineralBank_(base, 5);
    base.ConvertMinerals();
    CHECK(base.GetResources().GetMineralBank() == 0);
    CHECK(base.GetProduction().GetMineralStockpile() == stockpileBefore + 5);
    CHECK(base.GetProduction().GetCurrentProduction() == pFacility);
}

// ConvertMinerals claims leftover minerals; ApplyProduction must not also drain the bank,
// or a skipped conversion (or a bank filled after that stage) vanishes instead of landing
// next turn.
TEST_CASE("ApplyProduction does not drain the mineral bank for a real production item",
          "[production][stockpile]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);
    const BuildingConfig_t* pFacility = fixtures.buildings().Find("test_facility_a");
    REQUIRE(pFacility != nullptr);

    base.GetProduction().SetProduction(pFacility);
    LeaveMineralBank_(base, 5);
    (void)base.ApplyProduction();
    CHECK(base.GetResources().GetMineralBank() == 5);
}
