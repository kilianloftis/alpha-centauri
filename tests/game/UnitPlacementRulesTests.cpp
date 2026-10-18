#include "GameFixtures.h"

#include "game/base/BaseManager.h"
#include "game/faction/Faction.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitPlacementRules.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using namespace ac;
using namespace actest;

TEST_CASE("A grant is homed at its own context base, else the base nearest the tile",
          "[units][grant]")
{
    FactionFixture fixture;
    Faction& player = fixture.MakeFaction();
    Faction& rival = fixture.MakeFaction();
    BaseManager& first = fixture.MakeFactionBase(player, 4, 4);
    BaseManager& second = fixture.MakeFactionBase(player, 12, 12);
    BaseManager& rivalBase = fixture.MakeFactionBase(rival, 8, 4);

    CHECK(GrantAnchorBase(player, &second, &fixture.At(4, 6)) == second.GetBaseId());
    CHECK(GrantAnchorBase(player, &rivalBase, &fixture.At(11, 11)) == second.GetBaseId());
    CHECK(GrantAnchorBase(player, nullptr, &fixture.At(5, 5)) == first.GetBaseId());
    CHECK(GrantAnchorBase(rival, nullptr, nullptr) == rivalBase.GetBaseId());
}

TEST_CASE("Granted units take the origin, then whole rings out to radius 2", "[units][grant]")
{
    FactionFixture fixture;
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }
    const FactionId_t factionId = fixture.MakeFaction().GetFactionId();
    const UnitDesign& rLandDesign = fixture.MakeDesign({"test_chassis"});
    fixture.At(9, 9).SetElevation(-100);
    const Tile& origin = fixture.At(8, 8);

    const std::vector<const Tile*> one =
        PlacementTiles(origin, rLandDesign, factionId, 1, fixture.map);
    REQUIRE(one.size() == 1);
    CHECK(one.front() == &origin);

    // Ring 1 is taken whole, less the flooded tile a land unit cannot stand on.
    const std::vector<const Tile*> two =
        PlacementTiles(origin, rLandDesign, factionId, 2, fixture.map);
    CHECK(two.size() == 8);
    CHECK(std::find(two.begin(), two.end(), &fixture.At(9, 9)) == two.end());

    const std::vector<const Tile*> many =
        PlacementTiles(origin, rLandDesign, factionId, 100, fixture.map);
    CHECK(many.size() == 24);
}
