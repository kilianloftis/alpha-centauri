#include "ui/world/MapAppearance.h"

#include "game/Faction.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionTileMemory.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/map/Tile.h"

namespace ac
{

MapAppearance MapAppearance::Fogged(const WorldMap& rMap, const Faction* pViewer)
{
    return MapAppearance(rMap, pViewer, RememberedTiles_t::Fogged);
}

MapAppearance MapAppearance::Clear(const WorldMap& rMap, const Faction* pViewer)
{
    return MapAppearance(rMap, pViewer, RememberedTiles_t::Clear);
}

MapAppearance::MapAppearance(const WorldMap& rMap, const Faction* pViewer,
                             RememberedTiles_t remembered)
    : m_rMap(rMap)
    , m_pViewer(pViewer)
    , m_remembered(remembered)
{
}

TileCover_t MapAppearance::CoverOf(const Tile& rTile) const
{
    if (!m_pViewer)
    {
        return TileCover_t::None;
    }
    if (!m_pViewer->GetExploredMap().IsExplored(rTile))
    {
        return TileCover_t::Shroud;
    }
    if (m_remembered == RememberedTiles_t::Fogged && !m_pViewer->GetVisibleMap().IsVisible(rTile))
    {
        return TileCover_t::Fog;
    }
    return TileCover_t::None;
}

TileOccupants_t MapAppearance::OccupantsOf(const Tile& rTile) const
{
    if (m_pViewer && !m_pViewer->GetVisibleMap().IsVisible(rTile))
    {
        return m_pViewer->GetTileMemory().Occupants(rTile);
    }
    return TileOccupants_t{rTile.GetTerrainFeatures(), rTile.GetImprovements()};
}

} // namespace ac
