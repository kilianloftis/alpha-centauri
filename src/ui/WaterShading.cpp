#include "ui/WaterShading.h"

#include "game/map/MapUtils.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace ac
{

namespace
{

void RequireDepthShadeLandform_(const ImprovementRegistry& rOccupants, const char* pKey,
                                const std::string& rLandformId)
{
    const ImprovementConfig_t* pLandform = rOccupants.Find(rLandformId);
    if (!pLandform)
    {
        throw std::runtime_error(std::string("tile_renderer.water_shading.") + pKey + " '"
                                 + rLandformId + "' names no occupant");
    }
    if (!pLandform->art || pLandform->art->layer != ArtLayer_t::Landform
        || !pLandform->art->depthShade)
    {
        throw std::runtime_error(std::string("tile_renderer.water_shading.") + pKey + " '"
                                 + rLandformId + "' has no landform art with a depth_shade");
    }
}

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

int CornerShade_(const Tile& rTile, const WorldMap& rMap, DiamondCorner_t corner,
                 const WaterShadingStyle_t& rShading)
{
    double sum = DepthElevation_(rTile);
    int count = 1;
    for (const LatticeOffset_t& rOffset :
         k_CornerNeighbors[static_cast<std::size_t>(corner)])
    {
        if (const Tile* pNeighbor = GetTileAtLatticeOffset(rMap, rTile, rOffset.p, rOffset.q))
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

DiamondShades_t ResolveWaterShades(const Tile& rTile, const WorldMap& rMap,
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
    return DiamondShades_t{
        own,
        CornerShade_(rTile, rMap, DiamondCorner_t::West, rShading),
        CornerShade_(rTile, rMap, DiamondCorner_t::North, rShading),
        CornerShade_(rTile, rMap, DiamondCorner_t::East, rShading),
        CornerShade_(rTile, rMap, DiamondCorner_t::South, rShading),
    };
}

const std::string& SeaArtLandform(const DiamondShades_t& shades, const WaterShadingStyle_t& rShading)
{
    const bool bDeep = std::max({shades.west, shades.north, shades.east, shades.south})
                       >= rShading.deepFromShade;
    return bDeep ? rShading.deepLandform : rShading.shelfLandform;
}

void ValidateTerrainArtReferences(const WaterShadingStyle_t& rShading,
                                  const ImprovementRegistry& rOccupants)
{
    RequireDepthShadeLandform_(rOccupants, "deep_landform", rShading.deepLandform);
    RequireDepthShadeLandform_(rOccupants, "shelf_landform", rShading.shelfLandform);
    RequireDepthShadeLandform_(rOccupants, "coast_shades", rShading.coastShades);
}

void ApplyWaterShades(TileShape_t& rShape, const DiamondShades_t& shades,
                      const WaterShadeRange_t& range)
{
    if (range.max < 0)
    {
        throw std::invalid_argument("ApplyWaterShades: max shade is negative");
    }
    rShape.center.shade = ShadeIn_(shades.center, range);
    rShape.west.shade = ShadeIn_(shades.west, range);
    rShape.north.shade = ShadeIn_(shades.north, range);
    rShape.east.shade = ShadeIn_(shades.east, range);
    rShape.south.shade = ShadeIn_(shades.south, range);
}

} // namespace ac
