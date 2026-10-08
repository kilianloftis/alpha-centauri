#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/map/MapUtils.h"
#include "graphics/Graphics.h"

#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ac
{

class SpriteLibrary;
class Tile;
class WorldMap;
struct CoastCornerArt_t;
struct ImprovementConfig_t;
struct TileLayer_t;
struct TileRendererStyle_t;

// Stable variant index for a tile cell. Depends only on coords, content id, and count
// (config list order is the contract). Returns 0 when count is 0.
size_t PickSpriteIndex(int tileX, int tileY, std::string_view contentId, size_t count);

// paths[PickSpriteIndex(...)]; empty paths → empty string (caller skips the draw).
const std::string& PickSpritePath(const std::vector<std::string>& paths, int tileX, int tileY,
                                  std::string_view contentId);

// Shared terrain-tile cell drawing for the world map, location panel preview, and base
// workable-area ring.
// Fills by forest overlay or elevation (blue water / brown land), then draws
// ResolveTileLayers bottom-to-top. Terrain art is palette indices shaded as SMAC does: land by
// its relief shades, water per vertex by depth. Fungus is an overlay sprite (not a solid
// fill). Art that fails to load draws nothing under terrain (the fill shows) and a checker for
// objects. Footprint is a 2:1 isometric diamond (width = size, height = size / 2).
// When sprite_paths lists multiple assets, PickSpritePath chooses one per tile. Sprites load
// through the SpriteLibrary the renderer was built over; the style is the one it was built with.
class TileRenderer
{
public:
    // The tile's yield, for object sprites picked by yield (farm structures). Without one they
    // take their first row.
    using YieldLookup_t = std::function<TileResources_t(const Tile&)>;

    TileRenderer(SpriteLibrary& rSprites, const TileRendererStyle_t& rStyle);

    // Faction base art loads through the same library.
    SpriteLibrary& Sprites() { return m_rSprites; }

    // Fill used by the world map, location preview, and minimap (fog dims the fill).
    // Forest overrides the elevation gradient when present; fungus does not.
    Color_t FillColor(const Tile& rTile, bool bFogged = false) const;

    // Draws the tile on rShape: terrain on its four triangles (land at its vertex shades),
    // object sprites on a flat footprint seated at the mean of its corners. Fogged tiles
    // (explored memory on the world map) draw their land art fog_land_shade steps darker and get
    // a haze over the terrain layers; object sprites stay clear on top. pMap enables
    // neighbor-aware tile sets, coast overlays and water corner shading; null draws no coast
    // and shades water from the tile alone.
    void Render(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                bool bFogged = false, const WorldMap* pMap = nullptr,
                const YieldLookup_t& rYieldOf = {}) const;

    // Render's two halves, so the world map can draw a tile's grid lines between them as SMAC
    // does. RenderTerrain draws the fill, terrain layers (farm ground in place of the moisture
    // base), road networks, coast and fog haze; RenderObjects the tile bonuses and then every
    // improvement's object sprite (skipping those another occupant hides), seated at the mean of
    // the shape's four corners. An object whose configured art is missing draws a magenta and
    // black checker in its place.
    void RenderTerrain(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                       bool bFogged, const WorldMap* pMap) const;
    void RenderObjects(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                       const YieldLookup_t& rYieldOf = {}) const;

private:
    static constexpr std::size_t k_CoastPartCount = 2;
    static constexpr std::size_t k_CoastCaseCount = 8;
    using CoastPaths_t = std::array<std::array<std::array<std::string, k_CoastCaseCount>,
                                               k_DiamondCornerCount>,
                                    k_CoastPartCount>;

    static CoastPaths_t BuildCoastPaths_(const std::string& rDirectory);

    bool TryDrawSprite_(Graphics& rGraphics, const std::string& path, float x, float y,
                        float width, float height, const Color_t& tint) const;
    bool TryDrawTileSprite_(Graphics& rGraphics, const std::string& path,
                            const TileShape_t& rShape) const;
    TileShape_t TerrainShape_(const TileShape_t& rShape, const Tile& rTile, bool bFogged) const;
    const std::string& CoastSpritePath_(std::size_t part, const CoastCornerArt_t& rArt) const;
    void DrawCoastOverlay_(Graphics& rGraphics, const Tile& rTile, const WorldMap& rMap,
                           const TileShape_t& rShape, float shoreShade) const;
    bool TryDrawOccupantPath_(Graphics& rGraphics, const ImprovementConfig_t& rOccupant,
                              const std::string& path, const TileShape_t& rShape,
                              const Color_t& tint) const;
    bool TryDrawLayerSprite_(Graphics& rGraphics, const Tile& rTile, const TileLayer_t& rLayer,
                             const WorldMap* pMap, const TileShape_t& rShape) const;
    bool TryDrawWaterLandform_(Graphics& rGraphics, const Tile& rTile, const TileLayer_t& rLayer,
                               const WorldMap* pMap, const TileShape_t& rShape) const;
    void DrawLinkNetworks_(Graphics& rGraphics, const Tile& rTile, const WorldMap& rMap,
                           const TileShape_t& rShape) const;
    void DrawMissingArt_(Graphics& rGraphics, const TileShape_t& rShape) const;
    void DrawObject_(Graphics& rGraphics, const ImprovementConfig_t& rConfig,
                     const std::string& path, const TileShape_t& rShape) const;

    SpriteLibrary& m_rSprites;
    const TileRendererStyle_t& m_rStyle;
    CoastPaths_t m_coastPaths;
};

} // namespace ac
