#pragma once

#include "game/map/Tile.h"
#include <algorithm>
#include <cstdlib>
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

// Discrete Euclidean disk shared by base territory and the workable area:
// dx^2 + dy^2 <= radius^2 + 1. Radius 2 yields the classic SMAC 5x5-minus-corners
// workable cross (20 tiles around the base).
inline bool InEuclideanRadius(int dx, int dy, int radius)
{
    return dx * dx + dy * dy <= radius * radius + 1;
}

// King-move / square distance on a horizontally wrapping map (Y does not wrap):
// max(|DeltaX|, |dy|). Used by vision, aura radii, ZOC, and adjacent unit steps
// (see ForEachTileInChebyshevRadius).
inline int ChebyshevDistance(const Tile& rA, const Tile& rB, int mapWidth)
{
    return std::max(std::abs(DeltaX(rA.GetX(), rB.GetX(), mapWidth)),
                    std::abs(rA.GetY() - rB.GetY()));
}

inline bool AreChebyshevAdjacent(const Tile& rA, const Tile& rB, int mapWidth)
{
    return ChebyshevDistance(rA, rB, mapWidth) == 1;
}

// SMAC tabletop / "two-diagonal" distance on a horizontally wrapping map (Y does not wrap):
// longer + shorter/2, where longer/shorter are |DeltaX| and |dy|. Used by energy
// inefficiency (HQ distance) and unit-scrap closest-base. Integer half of the shorter
// leg (floor).
inline int TabletopDiagonalDistance(const Tile& rA, const Tile& rB, int mapWidth)
{
    const int dx = std::abs(DeltaX(rA.GetX(), rB.GetX(), mapWidth));
    const int dy = std::abs(rA.GetY() - rB.GetY());
    const int longer = std::max(dx, dy);
    const int shorter = std::min(dx, dy);
    return longer + shorter / 2;
}

// Orthogonal (4-way) neighbors in fixed order N, E, S, W. X wraps; Y may be null (skipped).
// Used by river downhill flow — not Chebyshev/diagonal.
template<typename WorldMapT, typename Fn>
void ForEachOrthogonalNeighbor(const Tile& rOrigin, WorldMapT& rWorldMap, Fn&& fn)
{
    static constexpr int k_Deltas[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (const auto& delta : k_Deltas)
    {
        auto* pTile = rWorldMap.GetTile(rOrigin.GetX() + delta[0], rOrigin.GetY() + delta[1]);
        if (pTile)
        {
            fn(pTile);
        }
    }
}

// Calls fn(tile_ptr, distance) for every tile within Chebyshev `radius` tiles of rOrigin
// (square / king-move distance: max(|dx|, |dy|)). This is the metric for vision and for
// all effect/improvement aura radii (Sensor, Condenser, unit auras, …).
// `distance` is the Chebyshev distance from rOrigin (>= 0).
// When includeOrigin is false (the common case for aura scans), rOrigin itself is skipped.
// When includeOrigin is true (e.g. RecomputeMoistureInRadius needs to cover the changed tile
// itself), rOrigin is included at distance 0.
//
// WorldMapT can be WorldMap or const WorldMap — the tile pointer type in the callback matches
// automatically (Tile* from WorldMap&, const Tile* from const WorldMap&).
//
// X wraps via WorldMap::GetTile; null tiles (Y out of bounds) are skipped before fn is invoked.
template<typename WorldMapT, typename Fn>
void ForEachTileInChebyshevRadius(const Tile& rOrigin, WorldMapT& rWorldMap,
                                   int radius, bool includeOrigin, Fn&& fn)
{
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            const int distance = std::max(std::abs(dx), std::abs(dy));
            if (distance > radius)
            {
                continue;
            }
            if (!includeOrigin && distance == 0)
            {
                continue;
            }

            auto* pTile = rWorldMap.GetTile(rOrigin.GetX() + dx, rOrigin.GetY() + dy);
            if (pTile)
            {
                fn(pTile, distance);
            }
        }
    }
}

// Calls fn(tile_ptr, distSq) for every tile in the discrete Euclidean disk
// dx^2 + dy^2 <= radius^2 + 1 (see InEuclideanRadius). `distSq` is dx^2 + dy^2.
// X wraps via WorldMap::GetTile.
template<typename WorldMapT, typename Fn>
void ForEachTileInEuclideanRadius(const Tile& rOrigin, WorldMapT& rWorldMap,
                                  const int radius, bool includeOrigin, Fn&& fn)
{
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            if (!InEuclideanRadius(dx, dy, radius))
            {
                continue;
            }
            if (!includeOrigin && dx == 0 && dy == 0)
            {
                continue;
            }

            auto* pTile = rWorldMap.GetTile(rOrigin.GetX() + dx, rOrigin.GetY() + dy);
            if (pTile)
            {
                fn(pTile, dx * dx + dy * dy);
            }
        }
    }
}

// The SMAC base workable area: Euclidean radius 2 (dx^2 + dy^2 <= 5), which is a 5x5
// square with the four corners removed. Skips rOrigin itself (20 surrounding tiles).
// WorldMapT can be WorldMap or const WorldMap. X wraps.
template<typename WorldMapT, typename Fn>
void ForEachTileInWorkableArea(const Tile& rOrigin, WorldMapT& rWorldMap, Fn&& fn)
{
    static constexpr int k_WorkableRadius = 2;
    ForEachTileInEuclideanRadius(rOrigin, rWorldMap, k_WorkableRadius, /*includeOrigin=*/false,
        [&](auto* pTile, int /*distSq*/) { fn(pTile); });
}

} // namespace ac
