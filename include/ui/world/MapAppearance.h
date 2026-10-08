#pragma once

#include "game/map/TileOccupants.h"

namespace ac
{

class Faction;
class Tile;
class WorldMap;

// What covers a tile when it is drawn.
enum class TileCover_t
{
    // Never explored: only the shroud shows.
    Shroud,
    // Explored but out of sight: what the viewer remembers, under fog.
    Fog,
    None,
};

// The map as one viewer sees it: live occupants on tiles in sight, remembered ones elsewhere,
// shroud where the viewer never explored. Without a viewer every tile is live and clear.
class MapAppearance
{
public:
    // Remembered tiles draw under fog.
    static MapAppearance Fogged(const WorldMap& rMap, const Faction* pViewer);
    // Remembered tiles draw clear, still showing what the viewer remembers.
    static MapAppearance Clear(const WorldMap& rMap, const Faction* pViewer);

    const WorldMap& Map() const { return m_rMap; }
    TileCover_t CoverOf(const Tile& rTile) const;
    TileOccupants_t OccupantsOf(const Tile& rTile) const;

private:
    enum class RememberedTiles_t
    {
        Fogged,
        Clear,
    };

    MapAppearance(const WorldMap& rMap, const Faction* pViewer, RememberedTiles_t remembered);

    const WorldMap& m_rMap;
    const Faction* m_pViewer;
    RememberedTiles_t m_remembered;
};

} // namespace ac
