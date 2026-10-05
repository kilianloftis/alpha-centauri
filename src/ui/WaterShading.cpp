#include "ui/WaterShading.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace ac
{

namespace
{

struct GridDelta_t
{
    int dx = 0;
    int dy = 0;
};

// The three neighbors that share each corner with the tile.
constexpr std::array<GridDelta_t, 3> k_WestCorner = {{{0, 1}, {-1, 1}, {-1, 0}}};
constexpr std::array<GridDelta_t, 3> k_NorthCorner = {{{-1, 0}, {-1, -1}, {0, -1}}};
constexpr std::array<GridDelta_t, 3> k_EastCorner = {{{0, -1}, {1, -1}, {1, 0}}};
constexpr std::array<GridDelta_t, 3> k_SouthCorner = {{{1, 0}, {1, 1}, {0, 1}}};

double DepthElevation_(const Tile& rTile)
{
    const ElevationRulesConfig_t& rRules = rTile.MapRules();
    return std::clamp(rTile.GetElevation(), rRules.minElevationMeters, rRules.oceanLevelMeters);
}

int ShadeAt_(double elevation, const ElevationRulesConfig_t& rRules,
             const std::vector<int>& depthShades)
{
    const double span =
        static_cast<double>(rRules.oceanLevelMeters - rRules.minElevationMeters);
    const double band = std::floor(static_cast<double>(depthShades.size())
                                   * (elevation - rRules.minElevationMeters) / span);
    const std::size_t index =
        std::min(static_cast<std::size_t>(std::max(band, 0.0)), depthShades.size() - 1);
    return depthShades[index];
}

int CornerShade_(const Tile& rTile, const WorldMap& rMap,
                 const std::array<GridDelta_t, 3>& neighbors, const std::vector<int>& depthShades)
{
    double sum = DepthElevation_(rTile);
    int count = 1;
    for (const GridDelta_t& rDelta : neighbors)
    {
        if (const Tile* pNeighbor =
                rMap.GetTile(rTile.GetX() + rDelta.dx, rTile.GetY() + rDelta.dy))
        {
            sum += DepthElevation_(*pNeighbor);
            ++count;
        }
    }
    return ShadeAt_(sum / count, rTile.MapRules(), depthShades);
}

const Color_t& TintAt_(int shade, const std::vector<Color_t>& tints)
{
    return tints[std::min(static_cast<std::size_t>(shade), tints.size() - 1)];
}

} // namespace

DiamondShades_t ResolveWaterShades(const Tile& rTile, const WorldMap* pMap,
                                   const std::vector<int>& depthShades)
{
    if (depthShades.empty())
    {
        throw std::invalid_argument("ResolveWaterShades: depthShades is empty");
    }
    const ElevationRulesConfig_t& rRules = rTile.MapRules();
    if (rRules.oceanLevelMeters <= rRules.minElevationMeters)
    {
        throw std::invalid_argument("ResolveWaterShades: the map's floor is not below ocean level");
    }
    const int own = ShadeAt_(DepthElevation_(rTile), rRules, depthShades);
    if (!pMap)
    {
        return DiamondShades_t{own, own, own, own, own};
    }
    return DiamondShades_t{
        own,
        CornerShade_(rTile, *pMap, k_WestCorner, depthShades),
        CornerShade_(rTile, *pMap, k_NorthCorner, depthShades),
        CornerShade_(rTile, *pMap, k_EastCorner, depthShades),
        CornerShade_(rTile, *pMap, k_SouthCorner, depthShades),
    };
}

DiamondTint_t WaterShadeTint(const DiamondShades_t& shades, const std::vector<Color_t>& tints)
{
    if (tints.empty())
    {
        throw std::invalid_argument("WaterShadeTint: tints is empty");
    }
    return DiamondTint_t{
        TintAt_(shades.center, tints), TintAt_(shades.west, tints), TintAt_(shades.north, tints),
        TintAt_(shades.east, tints),   TintAt_(shades.south, tints),
    };
}

} // namespace ac
