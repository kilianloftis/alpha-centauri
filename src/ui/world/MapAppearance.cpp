#include "ui/world/MapAppearance.h"

#include "game/Faction.h"
#include "game/faction/FactionTileMemory.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/map/Tile.h"

namespace ac
{

MapAppearance::MapAppearance(const WorldMap& rMap, const FactionVisibleMap* pVisible,
                             const FactionTileMemory* pMemory)
    : m_rMap(rMap)
    , m_pVisible(pVisible)
    , m_pMemory(pMemory)
{
}

TileOccupants_t MapAppearance::OccupantsOf(const Tile& rTile) const
{
    if (m_pVisible && m_pMemory && !m_pVisible->IsVisible(rTile))
    {
        return m_pMemory->Occupants(rTile);
    }
    return TileOccupants_t{rTile.GetTerrainFeatures(), rTile.GetImprovements()};
}

MapAppearance AppearanceOf(const WorldMap& rMap, const Faction* pFaction)
{
    if (!pFaction)
    {
        return MapAppearance(rMap, nullptr, nullptr);
    }
    return MapAppearance(rMap, &pFaction->GetVisibleMap(), &pFaction->GetTileMemory());
}

} // namespace ac
