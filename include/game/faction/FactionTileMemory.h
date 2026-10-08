#pragma once

#include "game/map/TileOccupants.h"
#include "lib/Revision.h"

#include <cstdint>
#include <vector>

namespace ac
{

class Tile;
struct ImprovementConfig_t;

// Per-faction memory of what each tile's occupants looked like when the faction last saw it:
// the terrain and improvement lists, copied by Record. Lives on Faction next to the explored
// map. Fogged tiles draw from here rather than from the live tile.
class FactionTileMemory
{
public:
    FactionTileMemory() = default;

    // Size (or resize) to the world. Forgets every tile.
    void Reset(int width, int height);

    // Copies the tile's current occupant lists. Throws before Reset.
    void Record(const Tile& rTile);

    // The lists last recorded for the tile; empty until it is first recorded. Throws before
    // Reset.
    TileOccupants_t Occupants(const Tile& rTile) const;

    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    struct Remembered_t
    {
        std::vector<const ImprovementConfig_t*> terrain;
        std::vector<const ImprovementConfig_t*> improvements;
    };

    size_t Index_(const Tile& rTile) const;

    int m_width = 0;
    int m_height = 0;
    std::vector<Remembered_t> m_tiles;
    Revision m_revision;
};

} // namespace ac
