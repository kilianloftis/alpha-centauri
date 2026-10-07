#pragma once

#include "game/faction/base/BaseTypes.h"
#include "graphics/Graphics.h"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ac
{

class Tile;
class WorldMap;

// Stable variant index for a tile cell. Depends only on coords, content id, and count
// (config list order is the contract). Returns 0 when count is 0.
size_t PickSpriteIndex(int tileX, int tileY, std::string_view contentId, size_t count);

// paths[PickSpriteIndex(...)]; empty paths → empty string (caller skips the draw).
const std::string& PickSpritePath(const std::vector<std::string>& paths, int tileX, int tileY,
                                  std::string_view contentId);

// Shared terrain-tile cell drawing for the world map and location panel preview.
// Fills by forest overlay or elevation (blue water / brown land), then draws
// ResolveTileLayers bottom-to-top. Terrain art is palette indices shaded as SMAC does: land by
// its relief shades, water per vertex by depth. Fungus is an overlay sprite (not a solid
// fill). Missing assets fall back to procedural moisture/rockiness cues (or a fungus fill if
// that sprite is absent). Footprint is a 2:1 isometric diamond (width = size, height = size / 2).
// When sprite_paths lists multiple assets, PickSpritePath chooses one per tile.
class TileRenderer
{
public:
    // The tile's yield, for object sprites picked by yield (farm structures). Without one they
    // take their first row.
    using YieldLookup_t = std::function<TileResources_t(const Tile&)>;

    // Fill used by the world map, location preview, and minimap (fog dims the fill).
    // Forest overrides the elevation gradient when present; fungus does not.
    static Color_t FillColor(const Tile& rTile, bool bFogged = false);

    // The flat diamond inscribed in (x, y, size, size / 2), at shade 0.
    static TileShape_t FlatTileShape(float x, float y, float size);

    // True when (px, py) lies in any of the shape's four triangles (centre to each edge).
    static bool ShapeContains(const TileShape_t& rShape, float px, float py);

    // Draws the tile on rShape: terrain on its four triangles (land at its vertex shades),
    // object sprites on a flat footprint seated at the mean of its corners. Fogged tiles
    // (explored memory on the world map) draw their land art fog_land_shade steps darker and get
    // a haze over the terrain layers; object sprites stay clear on top. pMap enables
    // neighbor-aware tile sets, rockiness insets, coast overlays and water corner shading; null
    // draws no coast and shades water from the tile alone.
    static void Render(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                       bool bFogged = false, const WorldMap* pMap = nullptr,
                       const YieldLookup_t& rYieldOf = {});

    // Render's two halves, so the world map can draw a tile's grid lines between them as SMAC
    // does. RenderTerrain draws the fill, terrain layers (farm ground in place of the moisture
    // base), road networks, coast and fog haze; RenderObjects the tile bonuses and then every
    // improvement's object sprite (skipping those another occupant hides), seated at the mean of
    // the shape's four corners. An object whose configured art is missing draws a magenta and
    // black checker in its place.
    static void RenderTerrain(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                              bool bFogged, const WorldMap* pMap);
    static void RenderObjects(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                              const YieldLookup_t& rYieldOf = {});
};

} // namespace ac
