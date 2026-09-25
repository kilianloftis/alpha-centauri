#pragma once

#include <string_view>

namespace ac
{

class Tile;

// Told after a tile's improvements or characteristics change. Tile does not know what the
// listener does with that. The session binds TileEffectsContext, which drops improvements
// CanBuildImprovement now rejects.
class TileChangeListener
{
public:
    virtual ~TileChangeListener() = default;

    // keepId is an improvement just added on this call. It stays through the first pass so
    // an incumbent that cannot share the tile is the one removed.
    virtual void OnTileChanged(Tile& rTile, std::string_view keepId) = 0;
};

} // namespace ac
