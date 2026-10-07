// Faction territory: Euclidean claim disk (dx^2+dy^2 <= R^2+1) from bases with
// same-terrain connectivity; contested tiles resolved by nearest base, then lower BaseId.

#include "GameFixtures.h"

#include "game/faction/base/BaseManager.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"

#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace ac;

namespace
{

void RebuildTerritory_(actest::FactionFixture& rFixture)
{
    std::vector<const BaseManager*> bases;
    for (const auto& pFaction : rFixture.factions)
    {
        for (const BaseManager& rBase : pFaction->Bases())
        {
            bases.push_back(&rBase);
        }
    }
    rFixture.map.GetTerritory().Rebuild(rFixture.map, bases);
}

} // namespace

TEST_CASE("Land base claims a Euclidean disk of contiguous land", "[territory]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    // Use the non-wrapping Y axis for radius tips — X wraps, so east/west tips on a
    // 9-wide map sit inside the disk via the short wrap path.
    fixture.MakeFactionBase(faction, 12, 4);
    RebuildTerritory_(fixture);

    const TerritoryMap& rTerritory = fixture.map.GetTerritory();
    const FactionId_t id = faction.GetFactionId();

    CHECK(rTerritory.GetOwner(12, 4) == id);
    // Cardinal tip: 7^2 = 49 <= 50.
    CHECK(rTerritory.GetOwner(5, 11) == id);
    // Boundary of the formula: 1^2+7^2 = 50 <= 50.
    CHECK(rTerritory.GetOwner(6, 12) == id);
    CHECK(rTerritory.GetOwner(3, 5) == id); // wrapped dx to x=0 is 4; 16+25=41 <= 50

    // Just outside: 2^2+7^2 = 53 > 50.
    CHECK_FALSE(rTerritory.HasOwner(7, 13));
    CHECK_FALSE(InEuclideanRadius(2, 7, 7));
}

TEST_CASE("Land territory does not cross water or claim sea tiles", "[territory]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    // A two-row water band blocks land connectivity north/south (Y does not wrap).
    for (const auto& pTile : fixture.map.GetTiles())
    {
        if (pTile->GetY() == 7 || pTile->GetY() == 8)
        {
            pTile->SetElevation(-100);
        }
    }
    // A sea tile adjacent to the base that must not be claimed by a land base.
    fixture.At(12, 4).SetElevation(-100);

    fixture.MakeFactionBase(faction, 11, 5);
    RebuildTerritory_(fixture);

    const TerritoryMap& rTerritory = fixture.map.GetTerritory();
    const FactionId_t id = faction.GetFactionId();

    CHECK(rTerritory.GetOwner(11, 5) == id);
    CHECK(rTerritory.GetOwner(10, 6) == id);
    CHECK_FALSE(rTerritory.HasOwner(12, 4)); // sea
    CHECK_FALSE(rTerritory.HasOwner(11, 7)); // water barrier itself
    CHECK_FALSE(rTerritory.HasOwner(9, 9));  // land beyond the cut
}

TEST_CASE("Sea base claims Euclidean radius-3 contiguous sea only", "[territory]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    for (const auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(-100);
    }
    // Land island that must not be claimed.
    fixture.At(6, 10).SetElevation(0);

    fixture.MakeFactionBase(faction, 8, 8);
    RebuildTerritory_(fixture);

    const TerritoryMap& rTerritory = fixture.map.GetTerritory();
    const FactionId_t id = faction.GetFactionId();

    CHECK(rTerritory.GetOwner(8, 8) == id);
    // 3^2 = 9 <= 10; 3^2+1^2 = 10 <= 10.
    CHECK(rTerritory.GetOwner(11, 5) == id);
    CHECK(rTerritory.GetOwner(11, 11) == id);
    CHECK(rTerritory.GetOwner(6, 12) == id); // dx=1,dy=3 -> 10
    // 4^2 = 16 > 10.
    CHECK_FALSE(rTerritory.HasOwner(12, 4));
    CHECK_FALSE(rTerritory.HasOwner(6, 10)); // land
}

TEST_CASE("Contested tiles go to the nearer base by Euclidean distance", "[territory]")
{
    actest::FactionFixture fixture;
    Faction& a = fixture.MakeFaction();
    Faction& b = fixture.MakeFaction();

    fixture.MakeFactionBase(a, 5, 5);
    fixture.MakeFactionBase(b, 11, 11);
    RebuildTerritory_(fixture);

    const TerritoryMap& rTerritory = fixture.map.GetTerritory();
    CHECK(rTerritory.GetOwner(6, 6) == a.GetFactionId());
    CHECK(rTerritory.GetOwner(10, 10) == b.GetFactionId());
    // Midpoint (4,4) is equidistant; A's base was founded first → lower BaseId.
    CHECK(rTerritory.GetOwner(8, 8) == a.GetFactionId());
}

TEST_CASE("Equidistant contested tiles prefer lower BaseId", "[territory]")
{
    actest::FactionFixture fixture;
    Faction& a = fixture.MakeFaction();
    Faction& b = fixture.MakeFaction();

    // B founds first → lower BaseId, wins the midpoint despite higher FactionId_t.
    BaseManager& baseB = fixture.MakeFactionBase(b, 11, 11);
    BaseManager& baseA = fixture.MakeFactionBase(a, 5, 5);
    REQUIRE(baseB.GetBaseId() < baseA.GetBaseId());

    RebuildTerritory_(fixture);
    CHECK(fixture.map.GetTerritory().GetOwner(8, 8) == b.GetFactionId());
}

TEST_CASE("Founding another base expands territory on rebuild", "[territory]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    fixture.MakeFactionBase(faction, 8, 0);
    RebuildTerritory_(fixture);
    REQUIRE(fixture.map.GetTerritory().GetOwner(8, 0) == faction.GetFactionId());
    // (8,8) from (0,0): wrapped dx=-1, dy=8 → 65 > 50, outside the first base's disk.
    REQUIRE_FALSE(fixture.map.GetTerritory().HasOwner(8, 16));

    fixture.MakeFactionBase(faction, 8, 16);
    RebuildTerritory_(fixture);
    CHECK(fixture.map.GetTerritory().GetOwner(8, 16) == faction.GetFactionId());
}

TEST_CASE("Workable area matches Euclidean radius 2", "[territory][workable]")
{
    // dx^2+dy^2 <= 5: includes (2,1), excludes (2,2) corners — same as the old cut-corners cross.
    CHECK(InEuclideanRadius(2, 1, 2));
    CHECK(InEuclideanRadius(1, 2, 2));
    CHECK_FALSE(InEuclideanRadius(2, 2, 2));
    CHECK(InEuclideanRadius(0, 2, 2));
}

TEST_CASE("Territory BFS wraps horizontally when the long path is water", "[territory][wrap]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    const int width = fixture.map.GetWidth();

    // Land only on the seam columns; the long way around is ocean.
    for (const auto& pTile : fixture.map.GetTiles())
    {
        const bool bSeamColumn = pTile->GetX() <= 1 || pTile->GetX() >= width - 2;
        pTile->SetElevation(bSeamColumn ? 100 : -100);
    }

    fixture.MakeFactionBase(faction, 0, 4);
    RebuildTerritory_(fixture);

    const TerritoryMap& rTerritory = fixture.map.GetTerritory();
    const FactionId_t id = faction.GetFactionId();
    CHECK(rTerritory.GetOwner(0, 4) == id);
    CHECK(rTerritory.GetOwner(width - 2, 4) == id);
    CHECK(rTerritory.GetOwner(width - 1, 5) == id);
    CHECK_FALSE(rTerritory.HasOwner(4, 4)); // water
}

TEST_CASE("Contested tiles prefer the wrap-short base", "[territory][wrap]")
{
    actest::FactionFixture fixture;
    Faction& a = fixture.MakeFaction();
    Faction& b = fixture.MakeFaction();
    const int width = fixture.map.GetWidth();

    fixture.MakeFactionBase(a, 0, 4);
    fixture.MakeFactionBase(b, 4, 6);
    RebuildTerritory_(fixture);

    // (width-2,4) is wrap-adjacent to A and farther from B.
    CHECK(fixture.map.GetTerritory().GetOwner(width - 2, 4) == a.GetFactionId());
}
