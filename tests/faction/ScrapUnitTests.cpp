// Player scrap of a unit: production.json kinds.unit.default_scrap formula over listed
// mineral cost, paid as minerals to the closest friendly base when the unit is on this
// faction's territory. Combat / support DestroyUnit does not refund.

#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionConfigParser.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/production/ScrapPayout.h"
#include "game/faction/base/production/ScrapRefundCalculator.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfigParser.h"
#include "TempConfigFile.h"

#include <nlohmann/json.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace ac;
using namespace actest;

namespace
{

bool UnitStillLive_(const Faction& rFaction, UnitId_t unitId)
{
    for (const Unit& rUnit : rFaction.GetUnitManager().Units())
    {
        if (rUnit.GetUnitId() == unitId)
        {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("Scrapping a unit in faction territory refunds half its minerals to the closest base",
          "[unit][scrap]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    Unit& unit = fixture.MakeUnit(faction, 5, 4, {"test_chassis", "test_costly_weapon"}, &base);
    REQUIRE(unit.GetDesign().GetBaseCost() == 20);
    REQUIRE(fixture.map.GetTerritory().GetOwner(unit.GetTile()) == faction.GetFactionId());
    REQUIRE(base.GetProduction().GetMineralStockpile() == 0);
    REQUIRE(faction.QuoteScrapUnit(unit)->amount == 10);

    const UnitId_t id = unit.GetUnitId();
    CHECK(faction.ScrapUnit(unit) == 10);
    CHECK(base.GetProduction().GetMineralStockpile() == 10);
    CHECK(faction.GetEconomy().GetEnergy() == 0);
    CHECK_FALSE(UnitStillLive_(faction, id));
}

TEST_CASE("Scrapping a unit outside faction territory grants no minerals", "[unit][scrap]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 0);
    Unit& unit = fixture.MakeUnit(faction, 6, 7, {"test_chassis", "test_costly_weapon"}, &base);
    REQUIRE_FALSE(fixture.map.GetTerritory().HasOwner(unit.GetTile()));
    REQUIRE(faction.QuoteScrapUnit(unit)->amount == 0);

    const UnitId_t id = unit.GetUnitId();
    CHECK(faction.ScrapUnit(unit) == 0);
    CHECK(base.GetProduction().GetMineralStockpile() == 0);
    CHECK_FALSE(UnitStillLive_(faction, id));
}

TEST_CASE("Unit scrap minerals go to the closer of two friendly bases", "[unit][scrap]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& near = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& far = fixture.MakeFactionBase(faction, 6, 6);
    Unit& unit = fixture.MakeUnit(faction, 3, 2, {"test_chassis", "test_costly_weapon"}, &near);
    const int width = fixture.map.GetWidth();
    REQUIRE(TabletopDiagonalDistance(unit.GetTile(), near.GetTile(), width) == 1);
    REQUIRE(TabletopDiagonalDistance(unit.GetTile(), far.GetTile(), width) == 5);
    REQUIRE(fixture.map.GetTerritory().GetOwner(unit.GetTile()) == faction.GetFactionId());

    CHECK(faction.ScrapUnit(unit) == 10);
    CHECK(near.GetProduction().GetMineralStockpile() == 10);
    CHECK(far.GetProduction().GetMineralStockpile() == 0);
}

TEST_CASE("Equal tabletop-distance unit scrap prefers the lower base id", "[unit][scrap]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& first = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& second = fixture.MakeFactionBase(faction, 6, 2);
    REQUIRE(first.GetBaseId() < second.GetBaseId());
    Unit& unit = fixture.MakeUnit(faction, 4, 2, {"test_chassis", "test_costly_weapon"}, &first);
    const int width = fixture.map.GetWidth();
    REQUIRE(TabletopDiagonalDistance(unit.GetTile(), first.GetTile(), width) == 2);
    REQUIRE(TabletopDiagonalDistance(unit.GetTile(), second.GetTile(), width) == 2);

    CHECK(faction.ScrapUnit(unit) == 10);
    CHECK(first.GetProduction().GetMineralStockpile() == 10);
    CHECK(second.GetProduction().GetMineralStockpile() == 0);
}

TEST_CASE("Unit scrap closest base uses tabletop distance, not Euclidean", "[unit][scrap]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    // From (4,4): (7,7) and (8,4) are both tabletop 4; Euclidean squared is 18 vs 16
    // (would prefer (8,4)). Tabletop tie → lower BaseId.
    BaseManager& diagonal = fixture.MakeFactionBase(faction, 7, 7);
    BaseManager& orthogonal = fixture.MakeFactionBase(faction, 8, 4);
    REQUIRE(diagonal.GetBaseId() < orthogonal.GetBaseId());
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_costly_weapon"},
                                  &diagonal);
    const int width = fixture.map.GetWidth();
    REQUIRE(TabletopDiagonalDistance(unit.GetTile(), diagonal.GetTile(), width) == 4);
    REQUIRE(TabletopDiagonalDistance(unit.GetTile(), orthogonal.GetTile(), width) == 4);
    REQUIRE(fixture.map.GetTerritory().GetOwner(unit.GetTile()) == faction.GetFactionId());

    CHECK(faction.ScrapUnit(unit) == 10);
    CHECK(diagonal.GetProduction().GetMineralStockpile() == 10);
    CHECK(orthogonal.GetProduction().GetMineralStockpile() == 0);
}

TEST_CASE("Destroying a unit without scrap grants no minerals", "[unit][scrap]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    Unit& unit = fixture.MakeUnit(faction, 5, 4, {"test_chassis", "test_costly_weapon"}, &base);
    REQUIRE(fixture.map.GetTerritory().GetOwner(unit.GetTile()) == faction.GetFactionId());

    faction.GetUnitManager().DestroyUnit(unit);
    CHECK(base.GetProduction().GetMineralStockpile() == 0);
}

TEST_CASE("Production config loads unit default_scrap as minerals", "[unit][scrap][config]")
{
    const TempConfigFile file("unit_scrap.json", R"json({
        "retool_penalty_threshold": 10,
        "retool_penalty_percent": 50,
        "prototype_surcharge_percent": 50,
        "kinds": {
            "unit": {
                "default_scrap": {
                    "formula": "floor(minerals / 2)",
                    "refund_type": "minerals",
                    "refund_ceiling_percent": 100
                }
            }
        }
    })json");
    const ProductionConfig_t config = ProductionConfigParser{}.ParseConfig(file.Path());
    REQUIRE(config.kinds.at(ConstructableKind_t::Unit).defaultScrap.has_value());
    const ScrapKindConfig_t& rScrap = *config.kinds.at(ConstructableKind_t::Unit).defaultScrap;
    CHECK(rScrap.formula == "floor(minerals / 2)");
    CHECK(rScrap.refundType == StatId_t::Minerals);
    CHECK(rScrap.refundCeilingPercent == 100);
}

TEST_CASE("Scrapping a supply crawler refunds its full mineral cost", "[unit][scrap]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    Unit& crawler = fixture.MakeUnit(faction, 5, 4, {"test_chassis", "test_supply_crawler"}, &base);
    REQUIRE(crawler.GetDesign().GetBaseCost() == 5);
    REQUIRE(fixture.map.GetTerritory().GetOwner(crawler.GetTile()) == faction.GetFactionId());
    REQUIRE(faction.QuoteScrapUnit(crawler)->amount == 5);

    CHECK(faction.ScrapUnit(crawler) == 5);
    CHECK(base.GetProduction().GetMineralStockpile() == 5);
}

TEST_CASE("CreditScrapRefund pays treasury, production, and resource banks", "[unit][scrap]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    const BaseId_t baseId = base.GetBaseId();

    const auto pay = [&](int amount, StatId_t refundType, std::optional<BaseId_t> destBaseId) {
        return CreditScrapRefund(
            PlanScrapPayout(ScrapQuote_t{true, amount, refundType}, destBaseId), faction);
    };

    CHECK(pay(7, StatId_t::EnergyCredits, std::nullopt) == 7);
    CHECK(faction.GetEconomy().GetEnergy() == 7);

    CHECK(pay(4, StatId_t::Minerals, baseId) == 4);
    CHECK(base.GetProduction().GetMineralStockpile() == 4);

    CHECK(pay(3, StatId_t::Nutrients, baseId) == 3);
    CHECK(base.GetResources().ConsumeNutrients() == 3);

    CHECK(pay(2, StatId_t::Econ, baseId) == 2);
    CHECK(base.GetResources().ConsumeEcon() == 2);

    CHECK(pay(1, StatId_t::Labs, baseId) == 1);
    CHECK(base.GetResources().ConsumeLabs() == 1);

    CHECK(pay(6, StatId_t::Psych, baseId) == 6);
    CHECK(base.GetResources().GetPsych() == 6);
}

TEST_CASE("Planning a scrap payout zeroes a base-destined refund with no base", "[unit][scrap]")
{
    const ScrapPayout_t stranded =
        PlanScrapPayout(ScrapQuote_t{true, 9, StatId_t::Minerals}, std::nullopt);
    CHECK(stranded.amount == 0);
    CHECK_FALSE(stranded.destBaseId.has_value());

    // Energy credits reach the treasury with no base at all.
    const ScrapPayout_t treasury =
        PlanScrapPayout(ScrapQuote_t{true, 9, StatId_t::EnergyCredits}, std::nullopt);
    CHECK(treasury.amount == 9);
    CHECK_FALSE(treasury.destBaseId.has_value());

    CHECK_THROWS_WITH(PlanScrapPayout(ScrapQuote_t{true, 1, StatId_t::Attack}, 0),
                      Catch::Matchers::ContainsSubstring("not a scrap payout"));
    CHECK_THROWS_WITH(PlanScrapPayout(ScrapQuote_t{}, 0),
                      Catch::Matchers::ContainsSubstring("unavailable"));
}

TEST_CASE("Crediting a scrap refund to a base the faction does not hold throws", "[unit][scrap]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeFactionBase(faction, 4, 4);

    const ScrapPayout_t payout =
        PlanScrapPayout(ScrapQuote_t{true, 3, StatId_t::Minerals}, 9999);
    CHECK_THROWS_WITH(CreditScrapRefund(payout, faction),
                      Catch::Matchers::ContainsSubstring("not held by this faction"));
}

TEST_CASE("Unit component scrap parses formula and refund_type overrides", "[unit][scrap][config]")
{
    UnitComponentConfigParser parser;
    const nlohmann::json j = nlohmann::json::parse(R"json({
        "id": "scrap_crawler",
        "name": "Scrap Crawler",
        "type": "weapon",
        "scrap": { "formula": "minerals", "refund_type": "energy_credits" }
    })json");
    const UnitComponentConfig_t config = parser.ParseComponentConfig(j);
    REQUIRE(config.scrap.has_value());
    CHECK(config.scrap->formula == "minerals");
    CHECK(config.scrap->refundType == StatId_t::EnergyCredits);
}
