#pragma once

namespace ac
{

class Tile;
class WorldMap;

// True if any improvement or terrain feature on the tile has terminatesRiver.
bool TileTerminatesRiver(const Tile& rTile);

// Mark HasRiver along orthogonal downhill flow from origin until water, local min,
// or a terminates_river feature. The terminus tile keeps HasRiver.
void TraceRiverFrom(Tile& rOrigin, WorldMap& rWorld);

// Clear all HasRiver flags, then reflow from every HasAquifer tile.
void RecomputeRivers(WorldMap& rWorld);

} // namespace ac
