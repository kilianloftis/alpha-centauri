#pragma once

#include <cstdint>

namespace ac
{

class Tile;
class WorldMap;

// Orthogonal river-neighbor mask for rendering (line stubs today; sprite selection later).
// Bits match ForEachOrthogonalNeighbor order: N, E, S, W.
enum class RiverConnection_t : uint8_t
{
    None = 0,
    North = 1u << 0,
    East = 1u << 1,
    South = 1u << 2,
    West = 1u << 3,
};

inline constexpr RiverConnection_t operator|(RiverConnection_t a, RiverConnection_t b)
{
    return static_cast<RiverConnection_t>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline constexpr RiverConnection_t operator&(RiverConnection_t a, RiverConnection_t b)
{
    return static_cast<RiverConnection_t>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline constexpr RiverConnection_t& operator|=(RiverConnection_t& a, RiverConnection_t b)
{
    a = a | b;
    return a;
}

inline constexpr bool HasRiverConnection(RiverConnection_t mask, RiverConnection_t bit)
{
    return (mask & bit) != RiverConnection_t::None;
}

// Which orthogonal neighbors also have a river. Returns None if rTile has no river.
RiverConnection_t GetRiverConnections(const Tile& rTile, const WorldMap& rWorld);

// True if any improvement or terrain feature on the tile has terminatesRiver.
bool TileTerminatesRiver(const Tile& rTile);

// Mark HasRiver along orthogonal downhill flow from origin until water, local min,
// or a terminates_river feature. The terminus tile keeps HasRiver.
void TraceRiverFrom(Tile& rOrigin, WorldMap& rWorld);

// Clear all HasRiver flags, then reflow from every HasAquifer tile.
void RecomputeRivers(WorldMap& rWorld);

} // namespace ac
