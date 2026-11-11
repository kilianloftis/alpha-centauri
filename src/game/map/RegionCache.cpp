#include "game/map/RegionCache.h"

#include "game/map/RegionRules.h"
#include "game/map/WorldMap.h"

#include <algorithm>

namespace ac
{

RegionCache::RegionCache(const WorldMap& rMap)
    : m_rMap(rMap)
{}

int RegionCache::RegionOf(const Tile& rTile) const
{
    Validate_();
    const int index = m_rMap.GetTileIndex(rTile);
    return m_regions[index];
}

int RegionCache::TileCountInRegion(const Tile& rTile) const
{
    Validate_();
    const int region = m_regions[m_rMap.GetTileIndex(rTile)];
    return m_regionTileCounts[region];
}

void RegionCache::Validate_() const
{
    const uint64_t revision = m_rMap.GetAppearanceRevision();
    if (m_builtAtRevision == revision)
    {
        return;
    }
    m_regions = AssignRegions(m_rMap);
    int maxRegion = 0;
    for (const int region : m_regions)
    {
        maxRegion = std::max(maxRegion, region);
    }
    m_regionTileCounts.assign(static_cast<std::size_t>(maxRegion) + 1, 0);
    for (const int region : m_regions)
    {
        ++m_regionTileCounts[region];
    }
    m_builtAtRevision = revision;
}

} // namespace ac
