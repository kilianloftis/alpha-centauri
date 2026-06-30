#pragma once

#include "game/map/Tile.h"
#include "game/map/TileLayer.h"
#include <array>

namespace ac
{

// Resolves a Tile's visual representation into an ordered array of layers.
// Layers are ordered from bottom (Landform) to top (Improvement).
// A content ID of std::nullopt indicates that nothing should be drawn for that layer.
std::array<TileLayer_t, k_tileLayerCount> ResolveTileLayers(const Tile& rTile);

} // namespace ac
