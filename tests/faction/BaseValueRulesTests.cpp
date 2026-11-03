#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/base/BaseManager.h"
#include "game/base/BaseRules.h"
#include "game/buildings/BuildingManager.h"
#include "game/diplomacy/BaseValueRules.h"
#include "game/faction/Faction.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace ac;
using namespace actest;

namespace
{

// The counterparty's base at (18, 8) is the one valued. The evaluator's headquarters and second
// base sit 11 rings away, past the scan, so with two evaluator bases in the region no region
// multiplier changes as bases are added and only the scanned distance moves.
struct BaseValueFixture
{
    FactionFixture fixtures;
    Faction& evaluator;
    Faction& counterparty;
    Faction& third;
    BaseManager& evaluatorHq;
    BaseManager& evaluatorBase;
    BaseManager& valued;

    BaseValueFixture()
        : evaluator(fixtures.MakeFaction())
        , counterparty(fixtures.MakeFaction())
        , third(fixtures.MakeFaction())
        , evaluatorHq(fixtures.MakeFactionBase(evaluator, 2, 2))
        , evaluatorBase(fixtures.MakeFactionBase(evaluator, 34, 14))
        , valued(fixtures.MakeFactionBase(counterparty, 18, 8))
    {
        evaluatorHq.GetBuildingManager().AddBuilding("Headquarters");
    }

    int Value() const
    {
        return BaseValue(valued, evaluator, counterparty);
    }

    // Floods the eight neighbours of (x, y), leaving it a one-tile island.
    void Isolate(int x, int y)
    {
        const Tile& rCentre = *fixtures.map.GetTile(x, y);
        for (const LatticeOffset_t& rStep : k_RingNeighbors)
        {
            GetTileAtLatticeOffset(fixtures.map, rCentre, rStep.p, rStep.q)->SetElevation(-100);
        }
    }
};

} // namespace

TEST_CASE("Headquarters distance measures to the headquarters, with a fallback without one",
          "[base][hq]")
{
    BaseValueFixture f;

    CHECK(HeadquartersDistance(f.evaluator, f.valued.GetTile()) == 13);
    CHECK(HeadquartersDistance(f.evaluator, f.evaluatorHq.GetTile()) == 0);
    CHECK(HeadquartersDistance(f.third, f.valued.GetTile()) == 12);
}

TEST_CASE("A counterparty base nearby lowers the value, unless it is across water",
          "[diplomacy][base_value]")
{
    BaseValueFixture f;
    const int alone = f.Value();

    SECTION("same region")
    {
        f.fixtures.MakeFactionBase(f.counterparty, 22, 8);
        CHECK(f.Value() < alone);
    }

    SECTION("another region")
    {
        f.Isolate(22, 8);
        f.fixtures.MakeFactionBase(f.counterparty, 22, 8);
        CHECK(f.Value() == alone);
    }
}

TEST_CASE("The evaluator's nearest base stops the scan", "[diplomacy][base_value]")
{
    BaseValueFixture f;
    const int alone = f.Value();

    SECTION("in the same region it raises the value and hides bases further out")
    {
        f.fixtures.MakeFactionBase(f.evaluator, 22, 8);
        const int nearEvaluator = f.Value();
        CHECK(nearEvaluator > alone);

        f.fixtures.MakeFactionBase(f.counterparty, 26, 8);
        CHECK(f.Value() == nearEvaluator);
    }

    SECTION("in another region it changes nothing but still hides bases further out")
    {
        f.Isolate(22, 8);
        f.fixtures.MakeFactionBase(f.evaluator, 22, 8);
        CHECK(f.Value() == alone);

        f.fixtures.MakeFactionBase(f.counterparty, 26, 8);
        CHECK(f.Value() == alone);
    }
}

TEST_CASE("A third faction's base in the region stops the scan", "[diplomacy][base_value]")
{
    BaseValueFixture f;
    const int alone = f.Value();

    f.fixtures.MakeFactionBase(f.third, 22, 8);
    CHECK(f.Value() == alone);

    f.fixtures.MakeFactionBase(f.counterparty, 26, 8);
    CHECK(f.Value() == alone);
}

TEST_CASE("The evaluator's headquarters has no base value", "[diplomacy][base_value]")
{
    BaseValueFixture f;

    CHECK_THROWS_AS(BaseValue(f.evaluatorHq, f.evaluator, f.counterparty), std::logic_error);
}

TEST_CASE("Any unit garrisons a base, and only the evaluator's units add their cost",
          "[diplomacy][base_value]")
{
    BaseValueFixture f;

    SECTION("the counterparty's base and units")
    {
        const int empty = f.Value();
        f.fixtures.MakeUnit(f.counterparty, 18, 8);
        const int garrisoned = f.Value();
        CHECK(garrisoned >= 2 * empty);
        CHECK(garrisoned <= 2 * empty + 1);

        f.fixtures.MakeUnit(f.counterparty, 18, 8, {"test_chassis", "test_costly_weapon"});
        CHECK(f.Value() == garrisoned);
    }

    SECTION("the evaluator's base and units")
    {
        f.fixtures.MakeUnit(f.evaluator, 34, 14);
        const int garrisoned = BaseValue(f.evaluatorBase, f.evaluator, f.counterparty);

        const Unit& rCostly =
            f.fixtures.MakeUnit(f.evaluator, 34, 14, {"test_chassis", "test_costly_weapon"});
        const int cost = rCostly.GetDesign().GetBaseCost();
        REQUIRE(cost > 0);
        // The fixture's unit_rows_multiplier 2 over minerals_per_row 10.
        CHECK(BaseValue(f.evaluatorBase, f.evaluator, f.counterparty)
              == garrisoned + 2 * cost / 10);
    }
}

TEST_CASE("Facilities weigh by registry order; projects add and orbitals do not",
          "[diplomacy][base_value]")
{
    BaseValueFixture f;
    const int bare = f.Value();
    BuildingManager& rBuildings = f.valued.GetBuildingManager();

    rBuildings.AddBuilding("Network_Node");
    rBuildings.AddBuilding("Tachyon_Field");
    rBuildings.AddBuilding("Recreation_Commons");
    // buildings_loadable.json order: Tachyon_Field 12, Recreation_Commons 4, Network_Node 20.
    const int facilities = f.Value();
    CHECK(facilities == bare + (1 * 12 + 2 * 4 + 3 * 20) / 10);

    rBuildings.AddBuilding("test_valued_orbital");
    CHECK(f.Value() == facilities);

    // The fixture's project_rows_multiplier 25 over minerals_per_row 10.
    rBuildings.AddBuilding("test_valued_project");
    CHECK(f.Value() == facilities + 25 * 30 / 10);
}
