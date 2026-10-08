#include "ui/TileRelief.h"

#include "game/map/MapUtils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace ac
{

namespace
{

using TileLevels_t = DiamondValues_t<float>;

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
    if (rTile.IsWater())
    {
        return 0.0f;
    }
    return LevelsAbove_(rTile.GetElevation(), rTile.MapRules(), mode, rStyle);
}

float CornerLevels_(const Tile& rTile, const WorldMap& rMap, DiamondCorner_t corner,
                    ReliefMode_t mode, const ReliefStyle_t& rStyle)
{
    if (!rTile.IsLand())
    {
        return 0.0f;
    }
    float sum = CenterLevels_(rTile, mode, rStyle);
    for (const LatticeOffset_t& rOffset :
         k_CornerNeighbors[static_cast<std::size_t>(corner)])
    {
        const Tile* pNeighbor = GetTileAtLatticeOffset(rMap, rTile, rOffset.p, rOffset.q);
        if (!pNeighbor || !pNeighbor->IsLand())
        {
            return 0.0f;
        }
        sum += CenterLevels_(*pNeighbor, mode, rStyle);
    }
    return sum / 4.0f;
}

TileLevels_t Levels_(const Tile& rTile, const WorldMap& rMap, ReliefMode_t mode,
                     const ReliefStyle_t& rStyle)
{
    return TileLevels_t{
        CenterLevels_(rTile, mode, rStyle),
        CornerLevels_(rTile, rMap, DiamondCorner_t::West, mode, rStyle),
        CornerLevels_(rTile, rMap, DiamondCorner_t::North, mode, rStyle),
        CornerLevels_(rTile, rMap, DiamondCorner_t::East, mode, rStyle),
        CornerLevels_(rTile, rMap, DiamondCorner_t::South, mode, rStyle),
    };
}

float CornerLevel_(const TileLevels_t& rLevels, DiamondCorner_t corner)
{
    switch (corner)
    {
        case DiamondCorner_t::West:
            return rLevels.west;
        case DiamondCorner_t::North:
            return rLevels.north;
        case DiamondCorner_t::East:
            return rLevels.east;
        case DiamondCorner_t::South:
            return rLevels.south;
    }
    return 0.0f;
}

// Facet k lies between corners k and k + 1. Its shade is SMAC's
// −3·(s + t) / |(s, t)| with (s, t) the two corners' heights above the centre in quarter levels,
// turned a quarter turn per facet. Flooring the length at the full-shade rise fades gentler
// slopes; a floor of at most one quarter level keeps SMAC's whole-level results.
std::array<float, k_DiamondCornerCount> FacetShades_(const Tile& rTile, const TileLevels_t& rLevels,
                                                     const ReliefStyle_t& rStyle)
{
    std::array<float, k_DiamondCornerCount> shades{};
    if (!rTile.IsLand())
    {
        return shades;
    }
    std::array<float, k_DiamondCornerCount> rise{};
    for (std::size_t corner = 0; corner < k_DiamondCornerCount; ++corner)
    {
        rise[corner] =
            k_QuarterLevels
            * (CornerLevel_(rLevels, static_cast<DiamondCorner_t>(corner)) - rLevels.center);
    }
    const float fullShadeRise = k_QuarterLevels * rStyle.fullShadeRiseMeters / rStyle.levelMeters;
    for (std::size_t facet = 0; facet < k_DiamondCornerCount; ++facet)
    {
        const float a = rise[facet];
        const float b = rise[(facet + 1) % k_DiamondCornerCount];
        const float turned[k_DiamondCornerCount][2] = {{a, b}, {-b, a}, {-a, -b}, {b, -a}};
        const float s = turned[facet][0];
        const float t = turned[facet][1];
        shades[facet] = -3.0f * (s + t) / std::max(std::sqrt(s * s + t * t), fullShadeRise);
    }
    return shades;
}

// The two facets of each of the four tiles that meet at the tile's corner; off-map tiles count
// as unshaded, as in SMAC.
float CornerShade_(const Tile& rTile, const WorldMap& rMap, DiamondCorner_t corner,
                   const std::array<float, k_DiamondCornerCount>& ownFacets, ReliefMode_t mode,
                   const ReliefStyle_t& rStyle)
{
    const auto touching = [](const std::array<float, k_DiamondCornerCount>& facets,
                             std::size_t at) {
        return facets[(at + k_DiamondCornerCount - 1) % k_DiamondCornerCount] + facets[at];
    };
    const std::size_t cornerIndex = static_cast<std::size_t>(corner);
    float sum = touching(ownFacets, cornerIndex);
    const auto& neighbors = k_CornerNeighbors[cornerIndex];
    for (std::size_t i = 0; i < neighbors.size(); ++i)
    {
        const LatticeOffset_t& rOffset = neighbors[i];
        if (const Tile* pNeighbor = GetTileAtLatticeOffset(rMap, rTile, rOffset.p, rOffset.q))
        {
            sum += touching(
                FacetShades_(*pNeighbor, Levels_(*pNeighbor, rMap, mode, rStyle), rStyle),
                (cornerIndex + 1 + i) % k_DiamondCornerCount);
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
        lift(levels.west),
        lift(levels.north),
        lift(levels.east),
        lift(levels.south),
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
    const std::array<float, k_DiamondCornerCount> facets = FacetShades_(rTile, levels, rStyle);
    const auto landShade = [&rStyle](float slope, float vertexLevels) {
        return std::clamp(slope - rStyle.altitudeLightSteps * vertexLevels, -k_MaxLandShade,
                          k_MaxLandShade);
    };
    const auto cornerShade = [&](DiamondCorner_t corner) {
        return landShade(CornerShade_(rTile, rMap, corner, facets, mode, rStyle),
                         CornerLevel_(levels, corner));
    };
    return TileShades_t{
        landShade((facets[0] + facets[1] + facets[2] + facets[3]) / 4.0f, levels.center),
        cornerShade(DiamondCorner_t::West),
        cornerShade(DiamondCorner_t::North),
        cornerShade(DiamondCorner_t::East),
        cornerShade(DiamondCorner_t::South),
    };
}

} // namespace ac
