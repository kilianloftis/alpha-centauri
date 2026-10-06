#include "ui/TileRelief.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace ac
{

namespace
{

struct GridDelta_t
{
    int dx = 0;
    int dy = 0;
};

// Corners W, N, E, S. Each lists the three neighbors that share it with the tile; neighbor i
// meets the shared point at its own corner (corner + 1 + i) % 4.
constexpr std::size_t k_CornerCount = 4;
constexpr std::array<std::array<GridDelta_t, 3>, k_CornerCount> k_CornerNeighbors = {{
    {{{0, 1}, {-1, 1}, {-1, 0}}},
    {{{-1, 0}, {-1, -1}, {0, -1}}},
    {{{0, -1}, {1, -1}, {1, 0}}},
    {{{1, 0}, {1, 1}, {0, 1}}},
}};

// Quarter levels: SMAC's lighting inputs are level sums over a corner's four tiles.
constexpr float k_QuarterLevels = 4.0f;
// SMAC's land vertex shades run −4…4.
constexpr float k_MaxLandShade = 4.0f;

float LevelsAbove_(int elevation, const ElevationRulesConfig_t& rRules, ReliefMode_t mode,
                   const ReliefStyle_t& rStyle)
{
    if (mode == ReliefMode_t::Flat)
    {
        return 0.0f;
    }
    const float levels =
        static_cast<float>(elevation - rRules.oceanLevelMeters) / rStyle.levelMeters;
    return std::max(mode == ReliefMode_t::Stepped ? std::floor(levels) : levels, 0.0f);
}

float CenterLevels_(const Tile& rTile, ReliefMode_t mode, const ReliefStyle_t& rStyle)
{
    if (!rTile.IsLand())
    {
        return 0.0f;
    }
    return LevelsAbove_(rTile.GetElevation(), rTile.MapRules(), mode, rStyle);
}

float CornerLevels_(const Tile& rTile, const WorldMap& rMap, std::size_t corner,
                    ReliefMode_t mode, const ReliefStyle_t& rStyle)
{
    if (!rTile.IsLand())
    {
        return 0.0f;
    }
    float sum = CenterLevels_(rTile, mode, rStyle);
    for (const GridDelta_t& rDelta : k_CornerNeighbors[corner])
    {
        const Tile* pNeighbor = rMap.GetTile(rTile.GetX() + rDelta.dx, rTile.GetY() + rDelta.dy);
        if (!pNeighbor || !pNeighbor->IsLand())
        {
            return 0.0f;
        }
        sum += CenterLevels_(*pNeighbor, mode, rStyle);
    }
    return sum / 4.0f;
}

struct TileLevels_t
{
    float center = 0.0f;
    std::array<float, k_CornerCount> corners{};
};

TileLevels_t Levels_(const Tile& rTile, const WorldMap& rMap, ReliefMode_t mode,
                     const ReliefStyle_t& rStyle)
{
    TileLevels_t levels;
    levels.center = CenterLevels_(rTile, mode, rStyle);
    for (std::size_t corner = 0; corner < k_CornerCount; ++corner)
    {
        levels.corners[corner] = CornerLevels_(rTile, rMap, corner, mode, rStyle);
    }
    return levels;
}

// Facet k lies between corners k and k + 1. Its shade is SMAC's
// −3·(s + t) / |(s, t)| with (s, t) the two corners' heights above the centre in quarter levels,
// turned a quarter turn per facet. Flooring the length at the full-shade rise fades gentler
// slopes; a floor of at most one quarter level keeps SMAC's whole-level results.
std::array<float, k_CornerCount> FacetShades_(const Tile& rTile, const TileLevels_t& rLevels,
                                             const ReliefStyle_t& rStyle)
{
    std::array<float, k_CornerCount> shades{};
    if (!rTile.IsLand())
    {
        return shades;
    }
    std::array<float, k_CornerCount> rise{};
    for (std::size_t corner = 0; corner < k_CornerCount; ++corner)
    {
        rise[corner] = k_QuarterLevels * (rLevels.corners[corner] - rLevels.center);
    }
    const float fullShadeRise = k_QuarterLevels * rStyle.fullShadeRiseMeters / rStyle.levelMeters;
    for (std::size_t facet = 0; facet < k_CornerCount; ++facet)
    {
        const float a = rise[facet];
        const float b = rise[(facet + 1) % k_CornerCount];
        const float turned[k_CornerCount][2] = {{a, b}, {-b, a}, {-a, -b}, {b, -a}};
        const float s = turned[facet][0];
        const float t = turned[facet][1];
        shades[facet] = -3.0f * (s + t) / std::max(std::sqrt(s * s + t * t), fullShadeRise);
    }
    return shades;
}

// The two facets of each of the four tiles that meet at the tile's corner; off-map tiles count
// as unshaded, as in SMAC.
float CornerShade_(const Tile& rTile, const WorldMap& rMap, std::size_t corner,
                   const std::array<float, k_CornerCount>& ownFacets, ReliefMode_t mode,
                   const ReliefStyle_t& rStyle)
{
    const auto touching = [](const std::array<float, k_CornerCount>& facets, std::size_t at) {
        return facets[(at + k_CornerCount - 1) % k_CornerCount] + facets[at];
    };
    float sum = touching(ownFacets, corner);
    for (std::size_t i = 0; i < k_CornerNeighbors[corner].size(); ++i)
    {
        const GridDelta_t& rDelta = k_CornerNeighbors[corner][i];
        if (const Tile* pNeighbor =
                rMap.GetTile(rTile.GetX() + rDelta.dx, rTile.GetY() + rDelta.dy))
        {
            sum += touching(
                FacetShades_(*pNeighbor, Levels_(*pNeighbor, rMap, mode, rStyle), rStyle),
                (corner + 1 + i) % k_CornerCount);
        }
    }
    return sum / 8.0f;
}

} // namespace

float MaxTileLift(const ElevationRulesConfig_t& rRules, ReliefMode_t mode,
                  const ReliefStyle_t& rStyle)
{
    return LevelsAbove_(rRules.maxElevationMeters, rRules, mode, rStyle) * rStyle.liftPerLevelRatio;
}

TileLifts_t ResolveTileLifts(const Tile& rTile, const WorldMap& rMap, ReliefMode_t mode,
                             const ReliefStyle_t& rStyle)
{
    const TileLevels_t levels = Levels_(rTile, rMap, mode, rStyle);
    const auto lift = [&rStyle](float value) { return value * rStyle.liftPerLevelRatio; };
    return TileLifts_t{
        lift(levels.center),
        lift(levels.corners[0]),
        lift(levels.corners[1]),
        lift(levels.corners[2]),
        lift(levels.corners[3]),
    };
}

TileShades_t ResolveTileShades(const Tile& rTile, const WorldMap& rMap, ReliefMode_t mode,
                               const ReliefStyle_t& rStyle)
{
    if (mode == ReliefMode_t::Flat || !rTile.IsLand())
    {
        return TileShades_t{};
    }
    const TileLevels_t levels = Levels_(rTile, rMap, mode, rStyle);
    const std::array<float, k_CornerCount> facets = FacetShades_(rTile, levels, rStyle);
    const auto landShade = [&rStyle](float slope, float vertexLevels) {
        return std::clamp(slope - rStyle.altitudeLightSteps * vertexLevels, -k_MaxLandShade,
                          k_MaxLandShade);
    };
    const auto cornerShade = [&](std::size_t corner) {
        return landShade(CornerShade_(rTile, rMap, corner, facets, mode, rStyle),
                         levels.corners[corner]);
    };
    return TileShades_t{
        landShade((facets[0] + facets[1] + facets[2] + facets[3]) / 4.0f, levels.center),
        cornerShade(0),
        cornerShade(1),
        cornerShade(2),
        cornerShade(3),
    };
}

} // namespace ac
