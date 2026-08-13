// Stockpile production items convert minerals forever via MineralsConverted StatModifiers
// into nutrients / energy / econ / labs / psych. The first available stockpile (tech gate,
// load order) is the empty-queue fallback; if none exists, minerals are wasted.

#include "GameFixtures.h"

#include "game/IConstructable.h"
#include "game/Faction.h"
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

std::string WriteTempBuildings_(const std::string& rContents)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_stockpile_buildings.json";
    std::ofstream out(path);
    out << rContents;
    return path.string();
}

const char* k_MineralCacheJson_ = R"(
    { "id": "mineral_cache", "name": "Mineral Cache",
      "effects": [{
        "type": "StatModifier", "scope": "ThisBase",
        "parameters": { "stat": "minerals", "amount": 50, "op": "Add" }
      }] }
)";

std::string MineralsConverted_(const char* pStat, const char* pAmount)
{
    return std::string(R"({ "type": "StatModifier", "scope": "ThisBase",
        "parameters": { "stat": ")")
        + pStat + R"(", "amount": )" + pAmount
        + R"(, "amount_source": "MineralsConverted" } })";
}

std::string StockpileJson_(const char* pId, const char* pName, const char* pStat,
                           const char* pAmount, const char* pExtraFields = "")
{
    return std::string(R"({ "id": ")") + pId + R"(", "name": ")" + pName
        + R"(", "stockpile": true)" + pExtraFields + R"(,
        "effects": [)" + MineralsConverted_(pStat, pAmount) + R"(] })";
}

BaseManager& MakeBaseWithRegistry_(FactionFixture& rFixtures, Faction& rFaction,
                                   BuildingRegistry& rRegistry)
{
    auto pBase = std::make_unique<BaseManager>(
        rFaction, rFixtures.nextBaseId++, "TestBase", rFixtures.At(4, 4),
        rRegistry,
        *rFixtures.dataContext.socialRatingRegistry,
        *rFixtures.dataContext.popTypeRegistry,
        *rFixtures.dataContext.popTypeAvailabilityCalculator,
        *rFixtures.dataContext.growthConfig,
        *rFixtures.dataContext.productionConfig,
        *rFixtures.dataContext.popCompositionCalculator,
        nullptr,
        *rFixtures.ctx);
    BaseManager& rBase = *pBase;
    rFaction.AddBase(std::move(pBase));
    return rBase;
}

} // namespace

TEST_CASE("A new base queues the first available stockpile", "[production][stockpile]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);

    const BuildingConfig_t* pStockpile = fixtures.buildings().Find("Stockpile_Energy");
    REQUIRE(pStockpile != nullptr);
    CHECK(pStockpile->IsStockpile());
    CHECK(base.GetProduction().GetCurrentProduction() == pStockpile);
    CHECK(base.GetMineralCost() == 0);
    CHECK_FALSE(base.GetProduction().IsReadyToComplete(base.GetBaseEffects(), false));
}

TEST_CASE("Clearing production falls back to the first available stockpile",
          "[production][stockpile]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);
    const BuildingConfig_t* pStockpile = fixtures.buildings().Find("Stockpile_Energy");
    REQUIRE(pStockpile != nullptr);
    const BuildingConfig_t* pFacility = fixtures.buildings().Find("test_facility_a");
    REQUIRE(pFacility != nullptr);

    base.GetProduction().SetProduction(pFacility);
    REQUIRE(base.GetProduction().GetCurrentProduction() == pFacility);

    base.GetProduction().SetProduction(nullptr);
    CHECK(base.GetProduction().GetCurrentProduction() == pStockpile);
}

TEST_CASE("Stockpile Energy converts this turn's minerals to energy and never completes",
          "[production][stockpile]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);
    const BuildingConfig_t* pStockpile = fixtures.buildings().Find("Stockpile_Energy");
    REQUIRE(pStockpile != nullptr);
    REQUIRE(base.GetProduction().GetCurrentProduction() == pStockpile);

    base.GetProduction().SetMineralStockpile(7);
    LeaveMineralBank_(base, 5);
    const int energyBefore = faction.GetEconomy().GetEnergy();

    base.ConvertSurplusMinerals();
    // ceil(5 * 0.5) = 3
    CHECK(faction.GetEconomy().GetEnergy() == energyBefore + 3);
    CHECK(base.GetResources().GetMineralBank() == 0);

    const ProductionApplyResult_t applied = base.ApplyProduction();
    CHECK(applied.kind == ProductionApplyKind_t::InProgress);
    CHECK(base.GetProduction().GetCurrentProduction() == pStockpile);
    CHECK(base.GetProduction().GetMineralStockpile() == 7);
    CHECK_FALSE(base.GetProduction().IsReadyToComplete(base.GetBaseEffects(), false));
}

TEST_CASE("Stockpile conversion rounds up fractional MineralsConverted yield",
          "[production][stockpile]")
{
    BuildingRegistry registry;
    registry.Load(WriteTempBuildings_(
        std::string("[") + k_MineralCacheJson_ + ","
        + StockpileJson_("quarter_stock", "Quarter", "energy", "0.25")
        + "]"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = MakeBaseWithRegistry_(fixtures, faction, registry);

    auto convert = [&](int minerals, int expectedEnergy) {
        LeaveMineralBank_(base, minerals);
        const int energyBefore = faction.GetEconomy().GetEnergy();
        base.ConvertSurplusMinerals();
        CHECK(faction.GetEconomy().GetEnergy() == energyBefore + expectedEnergy);
    };

    convert(0, 0);
    convert(1, 1); // ceil(0.25)
    convert(4, 1);
    convert(5, 2);
}

TEST_CASE("A stockpile converts zero minerals to zero output", "[production][stockpile]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);

    LeaveMineralBank_(base, 0);
    const int energyBefore = faction.GetEconomy().GetEnergy();

    base.ConvertSurplusMinerals();
    CHECK(faction.GetEconomy().GetEnergy() == energyBefore);
    CHECK(base.ApplyProduction().kind == ProductionApplyKind_t::InProgress);
}

TEST_CASE("A stockpile cannot be constructed as a building", "[production][stockpile]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);

    CHECK_FALSE(base.GetBuildingManager().CanAddBuilding("Stockpile_Energy"));
    CHECK_THROWS_AS(base.GetBuildingManager().AddBuilding("Stockpile_Energy"), std::runtime_error);
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

    const BuildingConfig_t* pStockpile = fixtures.buildings().Find("Stockpile_Energy");
    REQUIRE(pStockpile != nullptr);

    const std::vector<const IConstructable*> available = pBase->GetConstructable();
    CHECK(std::find(available.begin(), available.end(), pStockpile) != available.end());
}

TEST_CASE("Stockpile effects modify conversion yield", "[production][stockpile]")
{
    BuildingRegistry registry;
    registry.Load(WriteTempBuildings_(
        std::string("[") + k_MineralCacheJson_ + R"(,
            { "id": "boosted_stock", "name": "Boosted", "stockpile": true,
              "effects": [
                { "type": "StatModifier", "scope": "ThisBase",
                  "parameters": { "stat": "energy", "amount": 0.5,
                                  "amount_source": "MineralsConverted" } },
                { "type": "StatModifier", "scope": "ThisBase",
                  "parameters": { "stat": "energy", "amount": 100, "op": "AddPercent" } }
              ] }
        ])"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = MakeBaseWithRegistry_(fixtures, faction, registry);
    REQUIRE(base.GetProduction().GetCurrentProduction() != nullptr);
    CHECK(base.GetProduction().GetCurrentProduction()->GetId() == "boosted_stock");

    LeaveMineralBank_(base, 5);
    const int energyBefore = faction.GetEconomy().GetEnergy();
    base.ConvertSurplusMinerals();
    // 5 * 0.5 = 2.5, AddPercent 100 → 5.0
    CHECK(faction.GetEconomy().GetEnergy() == energyBefore + 5);
}

TEST_CASE("A nutrient stockpile credits the nutrient bank", "[production][stockpile]")
{
    BuildingRegistry registry;
    registry.Load(WriteTempBuildings_(
        std::string("[") + k_MineralCacheJson_ + ","
        + StockpileJson_("stock_nutrients", "Stockpile Nutrients", "nutrients", "1")
        + "]"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = MakeBaseWithRegistry_(fixtures, faction, registry);

    LeaveMineralBank_(base, 4);
    (void)base.GetResources().ConsumeNutrients();
    base.ConvertSurplusMinerals();
    CHECK(base.GetResources().ConsumeNutrients() == 4);
}

TEST_CASE("A stockpile can credit more than one output stat", "[production][stockpile]")
{
    BuildingRegistry registry;
    registry.Load(WriteTempBuildings_(
        std::string("[") + k_MineralCacheJson_ + R"(,
            { "id": "split_stock", "name": "Split", "stockpile": true,
              "effects": [
                { "type": "StatModifier", "scope": "ThisBase",
                  "parameters": { "stat": "energy", "amount": 0.5,
                                  "amount_source": "MineralsConverted" } },
                { "type": "StatModifier", "scope": "ThisBase",
                  "parameters": { "stat": "nutrients", "amount": 0.25,
                                  "amount_source": "MineralsConverted" } }
              ] }
        ])"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = MakeBaseWithRegistry_(fixtures, faction, registry);

    LeaveMineralBank_(base, 4);
    (void)base.GetResources().ConsumeNutrients();
    const int energyBefore = faction.GetEconomy().GetEnergy();
    base.ConvertSurplusMinerals();
    CHECK(faction.GetEconomy().GetEnergy() == energyBefore + 2); // ceil(4 * 0.5)
    CHECK(base.GetResources().ConsumeNutrients() == 1);          // ceil(4 * 0.25)
}

TEST_CASE("Fallback skips a tech-gated stockpile until the tech is discovered",
          "[production][stockpile]")
{
    BuildingRegistry registry;
    registry.Load(WriteTempBuildings_(
        std::string("[") + k_MineralCacheJson_ + ","
        + StockpileJson_("gated_stock", "Gated", "labs", "1",
                         R"(, "required_tech": "advanced_build")")
        + ","
        + StockpileJson_("open_stock", "Open", "energy", "0.5")
        + "]"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = MakeBaseWithRegistry_(fixtures, faction, registry);
    CHECK(base.GetProduction().GetCurrentProduction()->GetId() == "open_stock");

    faction.GetResearch().AddDiscoveredTech("advanced_build");
    base.GetProduction().SetProduction(nullptr);
    CHECK(base.GetProduction().GetCurrentProduction()->GetId() == "gated_stock");
}

TEST_CASE("With no available stockpile, the queue stays empty and minerals are wasted",
          "[production][stockpile]")
{
    BuildingRegistry registry;
    registry.Load(WriteTempBuildings_(
        std::string("[") + k_MineralCacheJson_ + ","
        + StockpileJson_("gated_stock", "Gated", "energy", "0.5",
                         R"(, "required_tech": "advanced_build")")
        + "]"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = MakeBaseWithRegistry_(fixtures, faction, registry);
    CHECK_FALSE(base.GetProduction().HasProduction());

    LeaveMineralBank_(base, 5);
    const int energyBefore = faction.GetEconomy().GetEnergy();
    base.ConvertSurplusMinerals();
    CHECK_FALSE(base.GetProduction().HasProduction());
    CHECK(base.GetResources().GetMineralBank() == 0);
    CHECK(faction.GetEconomy().GetEnergy() == energyBefore);
    CHECK(base.ApplyProduction().kind == ProductionApplyKind_t::Idle);
}

TEST_CASE("ApplyProduction does not convert surplus minerals", "[production][stockpile]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);

    LeaveMineralBank_(base, 5);
    const int energyBefore = faction.GetEconomy().GetEnergy();
    REQUIRE(base.ApplyProduction().kind == ProductionApplyKind_t::InProgress);
    CHECK(faction.GetEconomy().GetEnergy() == energyBefore);
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
    const int energyBefore = faction.GetEconomy().GetEnergy();
    base.ApplyMineralSupport();
    CHECK(base.GetResources().GetMineralBank() == 4);

    base.ConvertSurplusMinerals();
    // ceil(4 * 0.5) = 2
    CHECK(faction.GetEconomy().GetEnergy() == energyBefore + 2);
    CHECK(base.GetResources().GetMineralBank() == 0);
}

TEST_CASE("Stockpile econ is collected this turn after surplus conversion",
          "[production][stockpile]")
{
    BuildingRegistry registry;
    registry.Load(WriteTempBuildings_(
        std::string("[") + k_MineralCacheJson_ + ","
        + StockpileJson_("stock_econ", "Stockpile Econ", "econ", "1")
        + "]"));

    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = MakeBaseWithRegistry_(fixtures, faction, registry);

    LeaveMineralBank_(base, 4);
    (void)base.GetResources().ConsumeEcon();
    base.ConvertSurplusMinerals();
    CHECK(faction.CollectIncome() == 4);
}

TEST_CASE("A real production item keeps leftover minerals for ApplyProduction",
          "[production][stockpile]")
{
    FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& base = fixtures.MakeFactionBase(faction, 4, 4);
    const BuildingConfig_t* pFacility = fixtures.buildings().Find("test_facility_a");
    REQUIRE(pFacility != nullptr);

    base.GetProduction().SetProduction(pFacility);
    LeaveMineralBank_(base, 5);
    base.ConvertSurplusMinerals();
    CHECK(base.GetResources().GetMineralBank() == 5);
    CHECK(base.GetProduction().GetCurrentProduction() == pFacility);
}
