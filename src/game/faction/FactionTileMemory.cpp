#include "game/faction/FactionTileMemory.h"

#include "game/map/MapUtils.h"
#include "game/map/Tile.h"

#include <stdexcept>

namespace ac
{

void FactionTileMemory::Reset(int width, int height)
{
    m_width = width;
    m_height = height;
    const size_t count = (width > 0 && height > 0)
        ? static_cast<size_t>(width) * static_cast<size_t>(height) / 2
        : 0;
    m_tiles.assign(count, Remembered_t{});
    m_revision.Bump();
}

size_t FactionTileMemory::Index_(const Tile& rTile) const
{
    if (m_tiles.empty())
    {
        throw std::logic_error("FactionTileMemory used before Reset");
    }
    return static_cast<size_t>(TileIndex(rTile.GetX(), rTile.GetY(), m_width));
}

void FactionTileMemory::Record(const Tile& rTile)
{
    Remembered_t& rRemembered = m_tiles.at(Index_(rTile));
    const std::vector<const ImprovementConfig_t*>& rTerrain = rTile.GetTerrainFeatures();
    const std::vector<const ImprovementConfig_t*>& rImprovements = rTile.GetImprovements();
    if (rRemembered.terrain == rTerrain && rRemembered.improvements == rImprovements)
    {
        return;
    }
    rRemembered.terrain = rTerrain;
    rRemembered.improvements = rImprovements;
    m_revision.Bump();
}

TileOccupants_t FactionTileMemory::Occupants(const Tile& rTile) const
{
    const Remembered_t& rRemembered = m_tiles.at(Index_(rTile));
    return TileOccupants_t{rRemembered.terrain, rRemembered.improvements};
}

} // namespace ac
