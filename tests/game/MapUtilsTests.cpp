#include "TestHelpers.h"

#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace ac;

namespace
{

constexpr int k_Width = 40;
constexpr int k_Height = 31;

struct ScreenStep_t
{
    const char* name;
    int p;
    int q;
    int dx;
    int dy;
};

// The eight neighbors by screen direction, NE first and clockwise, with the lattice offset
// each one keeps and the SMAC coordinate step it lands on.
constexpr ScreenStep_t k_ScreenSteps[] = {
    {"NE", 0, -1, 1, -1}, {"E", 1, -1, 2, 0},  {"SE", 1, 0, 1, 1},   {"S", 1, 1, 0, 2},
    {"SW", 0, 1, -1, 1},  {"W", -1, 1, -2, 0}, {"NW", -1, 0, -1, -1}, {"N", -1, -1, 0, -2},
};

} // namespace

TEST_CASE("TileIndex is row-major over half-width rows and rejects odd parity", "[map][maputils]")
{
    CHECK(TileIndex(0, 0, 8) == 0);
    CHECK(TileIndex(2, 0, 8) == 1);
    CHECK(TileIndex(6, 0, 8) == 3);
    CHECK(TileIndex(1, 1, 8) == 4);
    CHECK(TileIndex(7, 1, 8) == 7);
    CHECK(TileIndex(0, 2, 8) == 8);
    CHECK(TileIndex(5, 3, 8) == 14);
    CHECK(TileIndex(7, 3, 8) == 15);

    CHECK_THROWS_AS(TileIndex(1, 0, 8), std::invalid_argument);
    CHECK_THROWS_AS(TileIndex(0, 1, 8), std::invalid_argument);
    CHECK_THROWS_AS(TileIndex(3, 2, 8), std::invalid_argument);
}

TEST_CASE("Each unit lattice offset lands on the neighbor in its screen direction",
          "[map][maputils][lattice]")
{
    WorldMap map(k_Width, k_Height, actest::TestMapRules());
    const Tile& rOrigin = *map.GetTile(20, 14);

    for (const ScreenStep_t& rStep : k_ScreenSteps)
    {
        INFO(rStep.name);
        const Tile* pNeighbor = GetTileAtLatticeOffset(map, rOrigin, rStep.p, rStep.q);
        REQUIRE(pNeighbor);
        CHECK(pNeighbor->GetX() == rOrigin.GetX() + rStep.dx);
        CHECK(pNeighbor->GetY() == rOrigin.GetY() + rStep.dy);
    }

    SECTION("a lattice offset of zero is the origin")
    {
        CHECK(GetTileAtLatticeOffset(map, rOrigin, 0, 0) == &rOrigin);
    }

    SECTION("offsets wrap across the x seam")
    {
        const Tile& rEastEdge = *map.GetTile(k_Width - 2, 14);
        const Tile* pEast = GetTileAtLatticeOffset(map, rEastEdge, 1, -1);
        REQUIRE(pEast);
        CHECK(pEast->GetX() == 0);
        CHECK(pEast->GetY() == 14);

        const Tile& rWestEdge = *map.GetTile(0, 14);
        const Tile* pWest = GetTileAtLatticeOffset(map, rWestEdge, -1, 1);
        REQUIRE(pWest);
        CHECK(pWest->GetX() == k_Width - 2);
        CHECK(pWest->GetY() == 14);
    }

    SECTION("offsets past the poles are null")
    {
        const Tile& rTopRow = *map.GetTile(20, 0);
        CHECK(GetTileAtLatticeOffset(map, rTopRow, 0, -1) == nullptr);
        CHECK(GetTileAtLatticeOffset(map, rTopRow, -1, -1) == nullptr);

        const Tile& rBottomRow = *map.GetTile(20, k_Height - 1);
        CHECK(GetTileAtLatticeOffset(map, rBottomRow, 1, 0) == nullptr);
        CHECK(GetTileAtLatticeOffset(map, rBottomRow, 1, 1) == nullptr);
    }
}

TEST_CASE("LatticeDelta takes the shortest way round the seam", "[map][maputils][lattice]")
{
    WorldMap map(k_Width, k_Height, actest::TestMapRules());

    SECTION("across the seam matches the same step away from it")
    {
        const Tile& rEast = *map.GetTile(k_Width - 2, 10);
        const Tile& rWestOfSeam = *map.GetTile(0, 10);
        const LatticeDelta_t across = LatticeDelta(rEast, rWestOfSeam, k_Width);
        CHECK(across.p == 1);
        CHECK(across.q == -1);

        const LatticeDelta_t back = LatticeDelta(rWestOfSeam, rEast, k_Width);
        CHECK(back.p == -1);
        CHECK(back.q == 1);
    }

    SECTION("a diagonal step across the seam")
    {
        const Tile& rFrom = *map.GetTile(k_Width - 1, 9);
        const Tile& rTo = *map.GetTile(0, 10);
        const LatticeDelta_t delta = LatticeDelta(rFrom, rTo, k_Width);
        CHECK(delta.p == 1);
        CHECK(delta.q == 0);
    }

    SECTION("every screen step reports its own lattice offset")
    {
        const Tile& rOrigin = *map.GetTile(20, 14);
        for (const ScreenStep_t& rStep : k_ScreenSteps)
        {
            INFO(rStep.name);
            const Tile& rNeighbor = *map.GetTile(20 + rStep.dx, 14 + rStep.dy);
            const LatticeDelta_t delta = LatticeDelta(rOrigin, rNeighbor, k_Width);
            CHECK(delta.p == rStep.p);
            CHECK(delta.q == rStep.q);
        }
    }
}

TEST_CASE("Chebyshev and tabletop distance follow the lattice", "[map][maputils][distance]")
{
    WorldMap map(k_Width, k_Height, actest::TestMapRules());
    const Tile& rOrigin = *map.GetTile(20, 14);

    const auto at = [&](int p, int q) -> const Tile&
    {
        const Tile* pTile = GetTileAtLatticeOffset(map, rOrigin, p, q);
        REQUIRE(pTile);
        return *pTile;
    };

    SECTION("every unit step is adjacent at distance 1")
    {
        for (const ScreenStep_t& rStep : k_ScreenSteps)
        {
            INFO(rStep.name);
            const Tile& rNeighbor = at(rStep.p, rStep.q);
            CHECK(ChebyshevDistance(rOrigin, rNeighbor, k_Width) == 1);
            CHECK(AreChebyshevAdjacent(rOrigin, rNeighbor, k_Width));
            CHECK(TabletopDiagonalDistance(rOrigin, rNeighbor, k_Width) == 1);
        }
        CHECK_FALSE(AreChebyshevAdjacent(rOrigin, rOrigin, k_Width));
        CHECK_FALSE(AreChebyshevAdjacent(rOrigin, at(2, 0), k_Width));
    }

    SECTION("Chebyshev is max(|p|, |q|)")
    {
        CHECK(ChebyshevDistance(rOrigin, at(3, 1), k_Width) == 3);
        CHECK(ChebyshevDistance(rOrigin, at(-2, 4), k_Width) == 4);
        CHECK(ChebyshevDistance(rOrigin, at(-3, -3), k_Width) == 3);
        CHECK(ChebyshevDistance(rOrigin, at(0, 0), k_Width) == 0);
    }

    SECTION("tabletop is the longer axis plus half the shorter")
    {
        CHECK(TabletopDiagonalDistance(rOrigin, at(4, 0), k_Width) == 4);
        CHECK(TabletopDiagonalDistance(rOrigin, at(2, 2), k_Width) == 3);
        CHECK(TabletopDiagonalDistance(rOrigin, at(-3, 1), k_Width) == 3);
        CHECK(TabletopDiagonalDistance(rOrigin, at(3, -3), k_Width) == 4);
    }

    SECTION("distances take the short way across the seam")
    {
        const Tile& rEast = *map.GetTile(k_Width - 2, 14);
        const Tile& rAcross = *map.GetTile(4, 14);
        const LatticeDelta_t delta = LatticeDelta(rEast, rAcross, k_Width);
        REQUIRE(delta.p == 3);
        REQUIRE(delta.q == -3);
        CHECK(ChebyshevDistance(rEast, rAcross, k_Width) == 3);
        CHECK(TabletopDiagonalDistance(rEast, rAcross, k_Width) == 4);
    }
}

TEST_CASE("Radius walks cover the same lattice shapes as before", "[map][maputils][radius]")
{
    WorldMap map(k_Width, k_Height, actest::TestMapRules());
    const Tile& rOrigin = *map.GetTile(20, 14);

    SECTION("Chebyshev radius 1 and 2 are the 3x3 and 5x5 lattice squares")
    {
        int ringOne = 0;
        ForEachTileInChebyshevRadius(rOrigin, map, 1, false, [&](const Tile*, int distance)
        {
            CHECK(distance == 1);
            ++ringOne;
        });
        CHECK(ringOne == 8);

        int withOrigin = 0;
        ForEachTileInChebyshevRadius(rOrigin, map, 2, true, [&](const Tile*, int)
        {
            ++withOrigin;
        });
        CHECK(withOrigin == 25);
    }

    SECTION("Chebyshev radius never yields a tile twice")
    {
        std::set<const Tile*> seen;
        ForEachTileInChebyshevRadius(rOrigin, map, 3, true, [&](const Tile* pTile, int)
        {
            CHECK(seen.insert(pTile).second);
        });
        CHECK(seen.size() == 49);
    }

    SECTION("Euclidean radius 2 is the 5x5 square minus its corners")
    {
        int count = 0;
        ForEachTileInEuclideanRadius(rOrigin, map, 2, true, [&](const Tile*, int distSq)
        {
            CHECK(distSq <= 5);
            ++count;
        });
        CHECK(count == 21);
    }

    SECTION("the workable area is the 20 tiles around the origin")
    {
        std::set<const Tile*> workable;
        ForEachTileInWorkableArea(rOrigin, map, [&](const Tile* pTile)
        {
            workable.insert(pTile);
        });
        CHECK(workable.size() == 20);
        CHECK(workable.count(&rOrigin) == 0);

        for (const Tile* pTile : workable)
        {
            const LatticeDelta_t delta = LatticeDelta(rOrigin, *pTile, k_Width);
            CHECK(std::abs(delta.p) <= 2);
            CHECK(std::abs(delta.q) <= 2);
            CHECK(std::abs(delta.p) + std::abs(delta.q) <= 3);
        }
    }

    SECTION("the workable area wraps across the seam")
    {
        const Tile& rEdge = *map.GetTile(k_Width - 2, 14);
        int count = 0;
        ForEachTileInWorkableArea(rEdge, map, [&](const Tile*)
        {
            ++count;
        });
        CHECK(count == 20);
    }

    SECTION("a pole base loses the workable tiles past the edge")
    {
        const Tile& rPole = *map.GetTile(20, 0);
        int count = 0;
        ForEachTileInWorkableArea(rPole, map, [&](const Tile*)
        {
            ++count;
        });
        CHECK(count < 20);
        CHECK(count > 0);
    }

    SECTION("the Euclidean test keeps the SMAC disk shape")
    {
        CHECK(InEuclideanRadius(2, 1, 2));
        CHECK(InEuclideanRadius(1, 2, 2));
        CHECK(InEuclideanRadius(0, 2, 2));
        CHECK_FALSE(InEuclideanRadius(2, 2, 2));
    }
}

TEST_CASE("Orthogonal neighbors are the tiles across the diamond's edges", "[map][maputils]")
{
    WorldMap map(k_Width, k_Height, actest::TestMapRules());
    const Tile& rOrigin = *map.GetTile(20, 14);

    std::vector<std::pair<int, int>> seen;
    ForEachOrthogonalNeighbor(rOrigin, map, [&](const Tile* pTile)
    {
        seen.emplace_back(pTile->GetX() - rOrigin.GetX(), pTile->GetY() - rOrigin.GetY());
    });

    const std::vector<std::pair<int, int>> expected = {{1, -1}, {1, 1}, {-1, 1}, {-1, -1}};
    CHECK(seen == expected);
}
