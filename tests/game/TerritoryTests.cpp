// Faction territory: Euclidean claim disk (dx^2+dy^2 <= R^2+1) from bases with
// same-terrain connectivity; contested tiles resolved by nearest base, then pop, then FactionId.

#include "GameFixtures.h"

#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
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
    // Corner base so some on-map tiles fall outside radius 7 (dx^2+dy^2 <= 50).
    fixture.MakeFactionBase(faction, 0, 0);
    RebuildTerritory_(fixture);

    const TerritoryMap& rTerritory = fixture.map.GetTerritory();
    const FactionId id = faction.GetFactionId();

    CHECK(rTerritory.GetOwner(0, 0) == id);
    // Cardinal tip: 7^2 = 49 <= 50.
    CHECK(rTerritory.GetOwner(7, 0) == id);
    // Boundary of the formula: 7^2+1^2 = 50 <= 50.
    CHECK(rTerritory.GetOwner(7, 1) == id);
    CHECK(rTerritory.GetOwner(5, 5) == id); // 50 <= 50

    // Just outside: 7^2+2^2 = 53 > 50.
    CHECK_FALSE(rTerritory.HasOwner(7, 2));
    CHECK_FALSE(InEuclideanRadius(7, 2, 7));
}

TEST_CASE("Land territory does not cross water or claim sea tiles", "[territory]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    // Water column blocks land connectivity from x=1 to x=5.
    for (int y = 0; y < fixture.map.GetHeight(); ++y)
    {
        fixture.At(3, y).SetElevation(-100);
    }
    // A sea tile adjacent to the base that must not be claimed by a land base.
    fixture.At(1, 5).SetElevation(-100);

    fixture.MakeFactionBase(faction, 1, 4);
    RebuildTerritory_(fixture);

    const TerritoryMap& rTerritory = fixture.map.GetTerritory();
    const FactionId id = faction.GetFactionId();

    CHECK(rTerritory.GetOwner(1, 4) == id);
    CHECK(rTerritory.GetOwner(2, 4) == id);
    CHECK_FALSE(rTerritory.HasOwner(1, 5)); // sea
    CHECK_FALSE(rTerritory.HasOwner(3, 4)); // water barrier itself
    CHECK_FALSE(rTerritory.HasOwner(5, 4)); // land beyond the cut
}

TEST_CASE("Sea base claims Euclidean radius-3 contiguous sea only", "[territory]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    for (int y = 0; y < fixture.map.GetHeight(); ++y)
    {
        for (int x = 0; x < fixture.map.GetWidth(); ++x)
        {
            fixture.At(x, y).SetElevation(-100);
        }
    }
    // Land island that must not be claimed.
    fixture.At(4, 6).SetElevation(0);

    fixture.MakeFactionBase(faction, 4, 4);
    RebuildTerritory_(fixture);

    const TerritoryMap& rTerritory = fixture.map.GetTerritory();
    const FactionId id = faction.GetFactionId();

    CHECK(rTerritory.GetOwner(4, 4) == id);
    // 3^2 = 9 <= 10; 3^2+1^2 = 10 <= 10.
    CHECK(rTerritory.GetOwner(4, 1) == id);
    CHECK(rTerritory.GetOwner(7, 4) == id);
    CHECK(rTerritory.GetOwner(5, 7) == id); // dx=1,dy=3 -> 10
    // 4^2 = 16 > 10.
    CHECK_FALSE(rTerritory.HasOwner(4, 0));
    CHECK_FALSE(rTerritory.HasOwner(4, 6)); // land
}

TEST_CASE("Contested tiles go to the nearer base by Euclidean distance", "[territory]")
{
    actest::FactionFixture fixture;
    Faction& a = fixture.MakeFaction();
    Faction& b = fixture.MakeFaction();

    fixture.MakeFactionBase(a, 1, 4);
    fixture.MakeFactionBase(b, 7, 4);
    RebuildTerritory_(fixture);

    const TerritoryMap& rTerritory = fixture.map.GetTerritory();
    CHECK(rTerritory.GetOwner(2, 4) == a.GetFactionId());
    CHECK(rTerritory.GetOwner(6, 4) == b.GetFactionId());
    // Midpoint (4,4) is equidistant; equal starting pop -> lower FactionId (a).
    CHECK(rTerritory.GetOwner(4, 4) == a.GetFactionId());
}

TEST_CASE("Equidistant contested tiles prefer larger population then FactionId", "[territory]")
{
    actest::FactionFixture fixture;
    Faction& a = fixture.MakeFaction();
    Faction& b = fixture.MakeFaction();

    BaseManager& baseA = fixture.MakeFactionBase(a, 1, 4);
    BaseManager& baseB = fixture.MakeFactionBase(b, 7, 4);

    // Grow B so it wins the equidistant midpoint despite higher FactionId.
    baseB.GetPopulation().AddPop();
    REQUIRE(baseB.GetPopulation().GetSize() > baseA.GetPopulation().GetSize());

    RebuildTerritory_(fixture);
    CHECK(fixture.map.GetTerritory().GetOwner(4, 4) == b.GetFactionId());
}

TEST_CASE("Founding another base expands territory on rebuild", "[territory]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    fixture.MakeFactionBase(faction, 0, 0);
    RebuildTerritory_(fixture);
    REQUIRE(fixture.map.GetTerritory().GetOwner(0, 0) == faction.GetFactionId());
    // (8,8) from (0,0): 128 > 50, outside the first base's disk.
    REQUIRE_FALSE(fixture.map.GetTerritory().HasOwner(8, 8));

    fixture.MakeFactionBase(faction, 8, 8);
    RebuildTerritory_(fixture);
    CHECK(fixture.map.GetTerritory().GetOwner(8, 8) == faction.GetFactionId());
}

TEST_CASE("Workable area matches Euclidean radius 2", "[territory][workable]")
{
    // dx^2+dy^2 <= 5: includes (2,1), excludes (2,2) corners — same as the old cut-corners cross.
    CHECK(InEuclideanRadius(2, 1, 2));
    CHECK(InEuclideanRadius(1, 2, 2));
    CHECK_FALSE(InEuclideanRadius(2, 2, 2));
    CHECK(InEuclideanRadius(0, 2, 2));
}
