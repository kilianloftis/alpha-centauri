#include "GameFixtures.h"

#include "game/base/BaseManager.h"
#include "game/faction/Faction.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/UnitPlacementRules.h"

#include <catch2/catch_test_macros.hpp>

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
