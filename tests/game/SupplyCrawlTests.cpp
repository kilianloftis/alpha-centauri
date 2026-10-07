#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/Faction.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/map/Tile.h"
#include "game/map/WorkedTileIndex.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"

#include <catch2/catch_test_macros.hpp>
#include <variant>

using namespace ac;

TEST_CASE("Supply crawl claims a free tile and registers via home base", "[unit][supply]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 8, 8);

    Unit& crawler = fixture.MakeUnit(faction, 12, 12, {"test_chassis", "test_supply_crawler"}, &base);
    REQUIRE(crawler.GetFlag(RuleFlagId_t::SupplyCrawl));

    CHECK(crawler.TryStartSupplyCrawl(StatId_t::Minerals));
    REQUIRE(crawler.IsSupplyCrawling());
    CHECK(crawler.GetWorkedTile() == &crawler.GetTile());
    CHECK(fixture.map.GetWorkedTiles().IsWorked(crawler.GetTile()));
    CHECK(std::get<SupplyCrawlOrder_t>(*crawler.GetOrder()).resource == StatId_t::Minerals);
}

TEST_CASE("Supply crawl fails when the tile is already worked", "[unit][supply]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 8, 8);

    Tile& farmTile = fixture.At(9, 9);
    base.UserAssignBestAvailableWorker(&farmTile);
    REQUIRE(fixture.map.GetWorkedTiles().IsWorked(farmTile));

    Unit& crawler = fixture.MakeUnit(faction, 9, 9, {"test_chassis", "test_supply_crawler"}, &base);
    CHECK_FALSE(crawler.TryStartSupplyCrawl(StatId_t::Nutrients));
    CHECK_FALSE(crawler.IsSupplyCrawling());
    CHECK_FALSE(crawler.GetOrder().has_value());
}

TEST_CASE("Supply crawl fails without the flag or a home base", "[unit][supply]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 8, 8);

    Unit& noFlag = fixture.MakeUnit(faction, 12, 12, {"test_chassis"}, &base);
    CHECK_FALSE(noFlag.TryStartSupplyCrawl(StatId_t::Energy));

    Unit& noHome = fixture.MakeUnit(faction, 11, 11, {"test_chassis", "test_supply_crawler"}, nullptr);
    CHECK_FALSE(noHome.TryStartSupplyCrawl(StatId_t::Energy));
}

TEST_CASE("Clearing or replacing an order abandons a supply crawl claim", "[unit][supply]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 8, 8);

    Unit& crawler = fixture.MakeUnit(faction, 12, 12, {"test_chassis", "test_supply_crawler"}, &base);
    REQUIRE(crawler.TryStartSupplyCrawl(StatId_t::Nutrients));
    const Tile& crawled = crawler.GetTile();
    REQUIRE(fixture.map.GetWorkedTiles().IsWorked(crawled));

    crawler.ClearOrder();
    CHECK_FALSE(crawler.IsSupplyCrawling());
    CHECK_FALSE(fixture.map.GetWorkedTiles().IsWorked(crawled));

    REQUIRE(crawler.TryStartSupplyCrawl(StatId_t::Nutrients));
    crawler.SetOrder(MoveOrder_t{&fixture.At(11, 11)});
    CHECK_FALSE(crawler.IsSupplyCrawling());
    CHECK_FALSE(fixture.map.GetWorkedTiles().IsWorked(crawled));
}

TEST_CASE("Supply crawl credits only the chosen resource to the home base", "[unit][supply]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 8, 8);

    const int mineralsBefore = base.GetMineralProduction();
    const int nutrientsBefore = base.GetNutrientProduction();

    Tile& rocky = fixture.At(12, 12);
    rocky.SetRockiness(Rockiness_t::Rocky); // +2 minerals
    rocky.SetMoisture(Moisture_t::Wet);     // +2 nutrients (ignored when crawling minerals)

    Unit& crawler = fixture.MakeUnit(faction, 12, 12, {"test_chassis", "test_supply_crawler"}, &base);
    REQUIRE(crawler.TryStartSupplyCrawl(StatId_t::Minerals));

    CHECK(base.GetMineralProduction() == mineralsBefore + 2);
    CHECK(base.GetNutrientProduction() == nutrientsBefore);

    base.ProduceResources();
    CHECK(base.GetResources().ConsumeMinerals() >= mineralsBefore + 2);
}
