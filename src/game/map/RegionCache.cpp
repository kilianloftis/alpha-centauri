#include "game/map/RegionCache.h"

#include "game/map/RegionRules.h"
#include "game/map/WorldMap.h"

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

void RegionCache::Validate_() const
{
    const uint64_t revision = m_rMap.GetAppearanceRevision();
    if (m_builtAtRevision == revision)
    {
        return;
    }
    m_regions = AssignRegions(m_rMap);
    m_builtAtRevision = revision;
}

} // namespace ac
