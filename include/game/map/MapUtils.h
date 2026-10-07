#pragma once

#include "game/map/Tile.h"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

namespace ac
{

// Horizontal cylinder wrap: map X is continuous (planet wraps east/west). Y does not wrap.
// Returns x in [0, width). width must be > 0.
inline int WrapX(int x, int width)
{
    int wrapped = x % width;
    if (wrapped < 0)
    {
        wrapped += width;
    }
    return wrapped;
}

// Shortest signed horizontal delta from xFrom to xTo on a cylinder of the given width.
// Result is in (-width/2, width/2]. width must be > 0.
inline int DeltaX(int xFrom, int xTo, int mapWidth)
{
    int dx = xTo - xFrom;
    dx %= mapWidth;
    if (dx > mapWidth / 2)
    {
        dx -= mapWidth;
    }
    else if (dx < -(mapWidth - 1) / 2)
    {
        dx += mapWidth;
    }
    return dx;
}

// Row-major index into a SMAC-coordinate tile array of size width*height/2.
// width is the x wrap period (even). Throws when x+y is odd.
inline int TileIndex(int x, int y, int width)
{
    if ((x + y) & 1)
    {
        throw std::invalid_argument("TileIndex: (" + std::to_string(x) + ", " + std::to_string(y)
                                    + ") has odd parity");
    }
    return y * (width / 2) + x / 2;
}

// Lattice step (p, q): p toward screen SE, q toward screen SW. A lattice offset maps to
// map coordinates as (p - q, p + q).
struct LatticeDelta_t
{
    int p = 0;
    int q = 0;
};

inline LatticeDelta_t LatticeDelta(const Tile& rFrom, const Tile& rTo, int width)
{
    const int dx = DeltaX(rFrom.GetX(), rTo.GetX(), width);
    const int dy = rTo.GetY() - rFrom.GetY();
    return {(dx + dy) / 2, (dy - dx) / 2};
}

// Map offset of one lattice step from the origin, or null off the map / odd parity.
template<typename WorldMapT>
auto GetTileAtLatticeOffset(WorldMapT& rMap, const Tile& rOrigin, int p, int q)
    -> decltype(rMap.GetTile(0, 0))
{
    return rMap.GetTile(rOrigin.GetX() + p - q, rOrigin.GetY() + p + q);
}

// Discrete Euclidean disk on the square lattice: p^2 + q^2 <= radius^2 + 1.
// Radius 2 yields the classic SMAC 5x5-minus-corners workable cross (20 tiles around the base).
inline bool InEuclideanRadius(int p, int q, int radius)
{
    return p * p + q * q <= radius * radius + 1;
}

// King-move / square distance on the lattice: max(|p|, |q|) = (|dx| + |dy|) / 2.
// Used by vision, aura radii, ZOC, and adjacent unit steps.
inline int ChebyshevDistance(const Tile& rA, const Tile& rB, int mapWidth)
{
    const LatticeDelta_t d = LatticeDelta(rA, rB, mapWidth);
    return std::max(std::abs(d.p), std::abs(d.q));
}

inline bool AreChebyshevAdjacent(const Tile& rA, const Tile& rB, int mapWidth)
{
    return ChebyshevDistance(rA, rB, mapWidth) == 1;
}

// SMAC tabletop / "two-diagonal" distance on the lattice: longer + shorter/2 of |p| and |q|.
// Used by energy inefficiency (HQ distance) and unit-scrap closest-base.
inline int TabletopDiagonalDistance(const Tile& rA, const Tile& rB, int mapWidth)
{
    const LatticeDelta_t d = LatticeDelta(rA, rB, mapWidth);
    const int ap = std::abs(d.p);
    const int aq = std::abs(d.q);
    const int longer = std::max(ap, aq);
    const int shorter = std::min(ap, aq);
    return longer + shorter / 2;
}

// Orthogonal (4-way) neighbors across diamond edges, in fixed order N, E, S, W on screen.
// Lattice offsets keep today's screen directions. X wraps; Y may be null (skipped).
template<typename WorldMapT, typename Fn>
void ForEachOrthogonalNeighbor(const Tile& rOrigin, WorldMapT& rWorldMap, Fn&& fn)
{
    static constexpr int k_Deltas[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (const auto& delta : k_Deltas)
    {
        auto* pTile = GetTileAtLatticeOffset(rWorldMap, rOrigin, delta[0], delta[1]);
        if (pTile)
        {
            fn(pTile);
        }
    }
}

// Calls fn(tile_ptr, distance) for every tile within Chebyshev `radius` lattice steps of
// rOrigin. When includeOrigin is false, rOrigin itself is skipped.
// X wraps via GetTileAtLatticeOffset; null tiles (Y out of bounds) are skipped.
template<typename WorldMapT, typename Fn>
void ForEachTileInChebyshevRadius(const Tile& rOrigin, WorldMapT& rWorldMap,
                                   int radius, bool includeOrigin, Fn&& fn)
{
    for (int q = -radius; q <= radius; ++q)
    {
        for (int p = -radius; p <= radius; ++p)
        {
            const int distance = std::max(std::abs(p), std::abs(q));
            if (distance > radius)
            {
                continue;
            }
            if (!includeOrigin && distance == 0)
            {
                continue;
            }

            auto* pTile = GetTileAtLatticeOffset(rWorldMap, rOrigin, p, q);
            if (pTile)
            {
                fn(pTile, distance);
            }
        }
    }
}

// Calls fn(tile_ptr, distSq) for every tile in the discrete Euclidean disk on the lattice
// p^2 + q^2 <= radius^2 + 1 (see InEuclideanRadius). `distSq` is p^2 + q^2.
template<typename WorldMapT, typename Fn>
void ForEachTileInEuclideanRadius(const Tile& rOrigin, WorldMapT& rWorldMap,
                                  const int radius, bool includeOrigin, Fn&& fn)
{
    for (int q = -radius; q <= radius; ++q)
    {
        for (int p = -radius; p <= radius; ++p)
        {
            if (!InEuclideanRadius(p, q, radius))
            {
                continue;
            }
            if (!includeOrigin && p == 0 && q == 0)
            {
                continue;
            }

            auto* pTile = GetTileAtLatticeOffset(rWorldMap, rOrigin, p, q);
            if (pTile)
            {
                fn(pTile, p * p + q * q);
            }
        }
    }
}

// The SMAC base workable area: Euclidean radius 2 on the lattice (p^2 + q^2 <= 5).
// Skips rOrigin itself (20 surrounding tiles).
template<typename WorldMapT, typename Fn>
void ForEachTileInWorkableArea(const Tile& rOrigin, WorldMapT& rWorldMap, Fn&& fn)
{
    static constexpr int k_WorkableRadius = 2;
    ForEachTileInEuclideanRadius(rOrigin, rWorldMap, k_WorkableRadius, /*includeOrigin=*/false,
        [&](auto* pTile, int /*distSq*/) { fn(pTile); });
}

} // namespace ac
