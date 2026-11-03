#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace ac
{

class Tile;
class WorldMap;

// AssignRegions memoised against the map's appearance revision, which every land/water change
// bumps through Tile::SetElevation.
class RegionCache
{
public:
    explicit RegionCache(const WorldMap& rMap);
    RegionCache(const RegionCache&) = delete;
    RegionCache& operator=(const RegionCache&) = delete;

    int RegionOf(const Tile& rTile) const;

private:
    void Validate_() const;

    const WorldMap& m_rMap;
    mutable std::vector<int> m_regions;
    mutable std::optional<uint64_t> m_builtAtRevision;
};

} // namespace ac
