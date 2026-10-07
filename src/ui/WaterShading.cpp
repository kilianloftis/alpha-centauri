#include "ui/WaterShading.h"

#include "game/map/MapUtils.h"

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

// SMAC's depth detail: one step per detailMeters below ocean level, counted down from the end of
// the table.
int ShadeAt_(double elevation, const ElevationRulesConfig_t& rRules,
             const WaterShadingStyle_t& rShading)
{
    const std::vector<int>& rTable = rShading.depthShades;
    const double detail = std::floor(static_cast<double>(rTable.size())
                                     + (elevation - rRules.oceanLevelMeters) / rShading.detailMeters);
    const std::size_t index =
        std::min(static_cast<std::size_t>(std::max(detail, 0.0)), rTable.size() - 1);
    return rTable[index];
}

int CornerShade_(const Tile& rTile, const WorldMap& rMap,
                 const std::array<GridDelta_t, 3>& neighbors, const WaterShadingStyle_t& rShading)
{
    double sum = DepthElevation_(rTile);
    int count = 1;
    for (const GridDelta_t& rDelta : neighbors)
    {
        if (const Tile* pNeighbor = GetTileAtLatticeOffset(rMap, rTile, rDelta.dx, rDelta.dy))
        {
            sum += DepthElevation_(*pNeighbor);
            ++count;
        }
    }
    return ShadeAt_(sum / count, rTile.MapRules(), rShading);
}

float ShadeIn_(int shade, const WaterShadeRange_t& range)
{
    return static_cast<float>(std::clamp(shade + range.offset, 0, range.max));
}

} // namespace

DiamondShades_t ResolveWaterShades(const Tile& rTile, const WorldMap* pMap,
                                   const WaterShadingStyle_t& rShading)
{
    if (rShading.depthShades.empty())
    {
        throw std::invalid_argument("ResolveWaterShades: depthShades is empty");
    }
    if (rShading.detailMeters <= 0.0f)
    {
        throw std::invalid_argument("ResolveWaterShades: detailMeters is not positive");
    }
    const int own = ShadeAt_(DepthElevation_(rTile), rTile.MapRules(), rShading);
    if (!pMap)
    {
        return DiamondShades_t{own, own, own, own, own};
    }
    return DiamondShades_t{
        own,
        CornerShade_(rTile, *pMap, k_WestCorner, rShading),
        CornerShade_(rTile, *pMap, k_NorthCorner, rShading),
        CornerShade_(rTile, *pMap, k_EastCorner, rShading),
        CornerShade_(rTile, *pMap, k_SouthCorner, rShading),
    };
}

const std::string& SeaArtLandform(const DiamondShades_t& shades, const WaterShadingStyle_t& rShading)
{
    const bool bDeep = std::max({shades.west, shades.north, shades.east, shades.south})
                       >= rShading.deepFromShade;
    return bDeep ? rShading.deepLandform : rShading.shelfLandform;
}

void ApplyWaterShades(TileShape_t& rShape, const DiamondShades_t& shades,
                      const WaterShadeRange_t& range)
{
    if (range.max < 0)
    {
        throw std::invalid_argument("ApplyWaterShades: range.max is negative");
    }
    rShape.center.shade = ShadeIn_(shades.center, range);
    rShape.west.shade = ShadeIn_(shades.west, range);
    rShape.north.shade = ShadeIn_(shades.north, range);
    rShape.east.shade = ShadeIn_(shades.east, range);
    rShape.south.shade = ShadeIn_(shades.south, range);
}

} // namespace ac
